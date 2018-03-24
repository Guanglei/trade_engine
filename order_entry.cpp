#include "order_entry.h"

#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <stdio.h>
#include "strategy.h"


namespace trade_engine
{
	bool order_entry::start()
	{
		trader_api_->Init();

		{
			boost::mutex::scoped_lock lock(cv_mutex_);
			if(!connected_cv_.timed_wait(lock, boost::get_system_time() + boost::posix_time::seconds(25)))
			{
				LOGGER << "Order entry 25 seconds passed but still not connected, failed the start...";
				return false;
			}
		}

		LOGGER << " Order entry connected successfully, going to send login request...";

		CThostFtdcReqUserLoginField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, broker_id_.c_str());
		strcpy(req.UserID, investor_id_.c_str());
		strcpy(req.Password, password_.c_str());
		int ret = trader_api_->ReqUserLogin(&req, ++request_id_);
		if(ret != 0)
		{
			LOGGER << "OE failed to send login request...";
			return false;
		}

		{
			boost::mutex::scoped_lock lock(cv_mutex_);
			if(!login_cv_.timed_wait(lock, boost::get_system_time() + boost::posix_time::seconds(25)))
			{
				LOGGER << "25 seconds passed but still not login, failed the start...";
				return false;
			}
		}

		LOGGER << "Login successfully...";

		ReqSettlementInfoConfirm();

		return true;
	}

	//override virtual method
	void order_entry::OnFrontConnected()
	{
		LOGGER << "order_entry::OnFrontConnected...";
		connected_cv_.notify_all();
	}

	void order_entry::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
									 CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{	
		LOGGER << "order_entry::OnRspUserLogin...";

		if(IsErrorRspInfo(pRspInfo))
		{
			return;
		}
	
		front_id_ = pRspUserLogin->FrontID;
		session_id_ = pRspUserLogin->SessionID;
		order_ref_ = boost::lexical_cast<int>(pRspUserLogin->MaxOrderRef);
		login_cv_.notify_all();
    }

	void order_entry::ReqSettlementInfoConfirm()
	{
		CThostFtdcSettlementInfoConfirmField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, broker_id_.c_str());
		strcpy(req.InvestorID, investor_id_.c_str());
		
		if(trader_api_->ReqSettlementInfoConfirm(&req, ++request_id_) == 0)
		{
			LOGGER << "Succeed to send settlement info confirm request";
		}
		else
		{
			LOGGER << "Failed to send settlement info confirm request";
		}
	}

	void order_entry::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, 
												 CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{
		LOGGER << "order_entry::OnRspSettlementInfoConfirm...";
	}

	bool order_entry::send_inst_qry_request(const std::string& inst_id)
	{
		CThostFtdcQryInstrumentField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.InstrumentID, inst_id.c_str());
		if(trader_api_->ReqQryInstrument(&req, ++request_id_) == 0)
		{
			LOGGER << "Succeed to send instrument query request";
			return true;
		}
		else
		{
			LOGGER << "Failed to send instrument query request";
			return false;
		}
	}

	void order_entry::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, 
								CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{
	}

	bool order_entry::send_account_qry_request()
	{
		CThostFtdcQryTradingAccountField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, broker_id_.c_str());
		strcpy(req.InvestorID, investor_id_.c_str());
		if(trader_api_->ReqQryTradingAccount(&req, ++request_id_))
		{
			LOGGER << "Succeed to send account query request...";
			return true;
		}
		else
		{
			LOGGER << "Failed to send account query request...";
			return false;
		}
	}

	void order_entry::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount,
											 CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{
		if (!IsErrorRspInfo(pRspInfo))
		{
			LOGGER << "Account query response successful...";
			LOGGER << "--->>> account trading day: " <<  pTradingAccount->TradingDay;
			LOGGER << "--->>> account availability: " << pTradingAccount->Available;	
		}
	}

	bool order_entry::send_investor_pos_request(const std::string& inst_id)
	{
		CThostFtdcQryInvestorPositionField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, broker_id_.c_str());
		strcpy(req.InvestorID, investor_id_.c_str());
		strcpy(req.InstrumentID, inst_id.c_str());
		if(trader_api_->ReqQryInvestorPosition(&req, ++request_id_) ==0 )
		{
			LOGGER << "Succeed to send investor position query request...";
			return true;
		}
		else
		{
			LOGGER << "Failed to send investor position query request...";
			return false;
		}
	}

	void order_entry::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, 
												CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{
	}

	///请求查询合约保证金率
	/*void CTraderSpi::ReqQryInstrumentMarginRate()
	{
			//cerr << "--->>> 查询合约保证金率"<< endl;
			CThostFtdcQryInstrumentMarginRateField req;
			memset(&req, 0, sizeof(req));

			///经纪公司代码
			strcpy(req.BrokerID, BROKER_ID);
			///投资者代码
			strcpy(req.InvestorID, INVESTOR_ID);
			///合约代码
			strcpy(req.InstrumentID, INSTRUMENT_ID);
			//////投机套保标志
			req.HedgeFlag = THOST_FTDC_HF_Speculation;	//投机

			int iResult = pUserApi->ReqQryInstrumentMarginRate(&req, ++iRequestID);
			//cerr << "--->>> 查询合约保证金率: " << ((iResult == 0) ? " 成功" : " 失败") << endl;
	}*/

	///请求查询合约手续费率
	/*void CTraderSpi::ReqQryInstrumentCommissionRate()
	{
			//cerr << "--->>> 查询合约手续费率"<< endl;
			CThostFtdcQryInstrumentCommissionRateField req;
			memset(&req, 0, sizeof(req));

			///经纪公司代码
			strcpy(req.BrokerID, BROKER_ID);
			///投资者代码
			strcpy(req.InvestorID, INVESTOR_ID);
			///合约代码
			strcpy(req.InstrumentID, INSTRUMENT_ID);

			int iResult = pUserApi->ReqQryInstrumentCommissionRate(&req, ++iRequestID);
			//cerr << "--->>> 查询合约手续费率: " << ((iResult == 0) ? " 成功" : " 失败") << endl;
	}*/

	//market order
	bool order_entry::send_order_insert_request(const std::string& inst_id, char offset_flag,
											TThostFtdcDirectionType direction, TThostFtdcVolumeType qty, int& order_ref)
	{
		CThostFtdcInputOrderField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, broker_id_.c_str());
		strcpy(req.InvestorID, investor_id_.c_str());
		strcpy(req.InstrumentID, inst_id.c_str());
		snprintf(req.OrderRef, sizeof(req.OrderRef), "%d", ++order_ref_);
		req.OrderPriceType = THOST_FTDC_OPT_AnyPrice;
		req.Direction = direction;
		req.VolumeTotalOriginal = qty;
		req.TimeCondition = THOST_FTDC_TC_IOC;
		req.VolumeCondition = THOST_FTDC_VC_CV;
		req.ContingentCondition = THOST_FTDC_CC_Immediately;
		req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
		req.CombOffsetFlag[0] = offset_flag;
		req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;

		if(trader_api_->ReqOrderInsert(&req, ++request_id_) == 0)
		{
			LOGGER << "Succeed to send order insert request " << req.OrderRef;
			order_ref = order_ref_;
			return true;
		}
		else
		{
			LOGGER << "Failed to send order insert request " << req.OrderRef;
			return false;
		}
	}

	//limit order
	bool order_entry::send_order_insert_request(const std::string& inst_id, char offset_flag,
												TThostFtdcDirectionType direction,
												TThostFtdcVolumeType qty, TThostFtdcPriceType price, int& order_ref, bool is_ioc)
	{
		CThostFtdcInputOrderField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, broker_id_.c_str());
		strcpy(req.InvestorID, investor_id_.c_str());
		strcpy(req.InstrumentID, inst_id.c_str());
		snprintf(req.OrderRef, sizeof(req.OrderRef), "%d", ++order_ref_);
		req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		req.Direction = direction;
		req.VolumeTotalOriginal = qty;
		req.LimitPrice = price;
		req.TimeCondition = (is_ioc ? THOST_FTDC_TC_IOC : THOST_FTDC_TC_GFD);
		req.VolumeCondition = THOST_FTDC_VC_AV;
		req.ContingentCondition = THOST_FTDC_CC_Immediately;
		req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
		req.CombOffsetFlag[0] = offset_flag;
		req.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;

		if(trader_api_->ReqOrderInsert(&req, ++request_id_) == 0)
		{
			LOGGER << "Succeed to send order insert request " << req.OrderRef;
			order_ref = order_ref_;
			return true;
		}
		else
		{
			LOGGER << "Failed to send order insert request " << req.OrderRef;
			return false;
		}
	}

	bool order_entry::send_order_cancel_request(const std::string& inst_id, int order_ref)
	{
		CThostFtdcInputOrderActionField req;
		memset(&req, 0, sizeof(req));
		req.FrontID = front_id_;
		req.SessionID = session_id_;
		strcpy(req.BrokerID, broker_id_.c_str());
		strcpy(req.InvestorID, investor_id_.c_str());
		strcpy(req.InstrumentID, inst_id.c_str());
		snprintf(req.OrderRef, sizeof(req.OrderRef), "%d", order_ref_);

		if(trader_api_->ReqOrderAction(&req, ++request_id_) == 0)
		{
			LOGGER << "Succeed to send order cancel request " << req.OrderRef;
			return true;
		}
		else
		{
			LOGGER << "Failed to send order cancel request " << req.OrderRef;
			return false;
		}
	}

	void order_entry::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, 
									   CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{
		IsErrorRspInfo(pRspInfo);

		order_event e;
		e.type = order_event_type::rejected;
		e.order_ref = atoi(pInputOrder->OrderRef);
		e.direction = get_order_direction(pInputOrder->Direction);
		e.inst = instrument_factory::get_instance().create(pInputOrder->InstrumentID);
		e.offset = get_offset_flag(pInputOrder->CombOffsetFlag[0]);
		LOGGER << "Receive new order insert response... " << pRspInfo->ErrorID << " " << pRspInfo->ErrorMsg;
		//strat_->deliver_order_event(e);
	}

	//委托单
	void  order_entry::ReqParkedOrderInsert()
	{
	}

	void order_entry::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, 
									   CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{
		IsErrorRspInfo(pRspInfo);

		if(is_own_message(*pInputOrderAction))
		{
			order_event e;
			e.order_ref = atoi(pInputOrderAction->OrderRef);
			e.type = order_event_type::cxl_rejected;
			e.inst = instrument_factory::get_instance().create(pInputOrderAction->InstrumentID);
			
			//strat_->deliver_order_event(e);
		}
	}

	void order_entry::OnRtnOrder(CThostFtdcOrderField *pOrder)
	{
		if(is_own_message(*pOrder))
		{
			order_event e;
			e.order_ref = atoi(pOrder->OrderRef);
			e.direction = get_order_direction(pOrder->Direction);
			e.offset = get_offset_flag(pOrder->CombOffsetFlag[0]);
			e.price = pOrder->LimitPrice;
			e.quantity = pOrder->VolumeTotalOriginal;
			e.type = get_order_event_type(pOrder->OrderSubmitStatus, pOrder->OrderStatus);
			e.inst = instrument_factory::get_instance().create(pOrder->InstrumentID);

			LOGGER << "order_entry::OnRtnOrder... " << pOrder->StatusMsg;
			//strat_->deliver_order_event(e);
		}
	}

	void order_entry::OnRtnTrade(CThostFtdcTradeField *pTrade)
	{
		LOGGER << "--->>> trade: " <<pTrade->TradingDay <<"_"<< pTrade->TradeTime
			      << "_" << pTrade->InstrumentID << "_" << ((pTrade->Direction == '0') ? "买" : "卖")
				  << "_" <<  ((pTrade->OffsetFlag == '0') ? "开仓" : "平仓") << "_" 
				  << pTrade->Volume << "_" << pTrade->Price;

		order_event e;
		e.order_ref = atoi(pTrade->OrderRef);
		e.direction = get_order_direction(pTrade->Direction);
		e.offset = get_offset_flag(pTrade->OffsetFlag);
		e.type = order_event_type::fill;
		e.price = pTrade->Price;
		e.quantity = pTrade->Volume;
		e.inst = instrument_factory::get_instance().create(pTrade->InstrumentID);
		
		//strat_->deliver_order_event(e);
	}

	void order_entry::OnFrontDisconnected(int nReason)
	{
		LOGGER << "--->>> " << "OnFrontDisconnected!";
	}
		
	void order_entry::OnHeartBeatWarning(int nTimeLapse)
	{
		LOGGER << "--->>> " << "OnHeartBeatWarning";
		LOGGER << "--->>> nTimerLapse = " << nTimeLapse;
	}

	void order_entry::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{
		LOGGER << "--->>> " << "OnRspError, request id: " << nRequestID;
	}

	bool order_entry::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
	{
		bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
		if (bResult)
			LOGGER << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg;
		return bResult;
	}
}
