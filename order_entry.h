#ifndef TRADE_ENGINE_ORDER_ENTRY_H
#define TRADE_ENGINE_ORDER_ENTRY_H

#include "./ctp_api/ThostFtdcTraderApi.h"
#include "types.h"
#include <string.h>
#include <boost/thread/mutex.hpp>  
#include <boost/thread/condition.hpp>
#include "logger.h"

namespace trade_engine
{
	class strategy;

	class order_entry : public CThostFtdcTraderSpi
	{
	public:

		order_entry() : trader_api_(0), request_id_(0) {}

		bool initialize(const trade_config& config)
		{
			trader_api_ = CThostFtdcTraderApi::CreateFtdcTraderApi();
			if(!trader_api_)
			{
				LOGGER << "Failed to call CreateFtdcTraderApi...";
				return false;
			}
			trader_api_->RegisterSpi(this);
			trader_api_->SubscribePublicTopic(THOST_TERT_QUICK);					
			trader_api_->SubscribePrivateTopic(THOST_TERT_QUICK);					
				
			std::string front_addr = "tcp://" + config.trd_ip + ":" + config.trd_port;
			LOGGER << "Order entry front address: " << front_addr;
			trader_api_->RegisterFront(const_cast<char*>(front_addr.c_str()));						
		
			broker_id_ = config.broker_id;
			investor_id_ = config.investor_id;
			password_ = config.password;

			return true;
		}

		void set_strategy(strategy& strat)
		{
			strat_ = &strat;
		}

		bool start();

	public:

		bool send_inst_qry_request(const std::string& inst_id);
		bool send_account_qry_request();
		bool send_investor_pos_request(const std::string& inst_id);
		
		bool send_order_insert_request(const std::string& inst_id, char offset_flag,
									   TThostFtdcDirectionType direction,
									   TThostFtdcVolumeType qty, int& order_ref);

		bool send_order_insert_request(const std::string& inst_id, char offset_flag,
									   TThostFtdcDirectionType direction,
									   TThostFtdcVolumeType qty, TThostFtdcPriceType, int& order_ref, bool is_ioc = true);

		bool send_order_cancel_request(const std::string& inst_id, int order_ref);

	public:
		///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
		virtual void OnFrontConnected();

		///登录请求响应
		virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///投资者结算结果确认响应
		virtual void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	
		///请求查询合约响应
		virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///请求查询资金账户响应
		virtual void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///请求查询投资者持仓响应
		virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///报单录入请求响应
		virtual void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///请求查询合约保证金率响应
		//virtual void OnRspQryInstrumentMarginRate(CThostFtdcInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	
		///请求查询合约手续费率响应
		//virtual void OnRspQryInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField *pInstrumentCommissionRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///报单操作请求响应
		virtual void OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///错误应答
		virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	
		///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
		virtual void OnFrontDisconnected(int nReason);
		
		///心跳超时警告。当长时间未收到报文时，该方法被调用。
		virtual void OnHeartBeatWarning(int nTimeLapse);
	
		///报单通知
		virtual void OnRtnOrder(CThostFtdcOrderField *pOrder);

		///成交通知
		virtual void OnRtnTrade(CThostFtdcTradeField *pTrade);

	private:
		void ReqSettlementInfoConfirm();
		//预埋单录入请求
		void ReqParkedOrderInsert();
		//预埋撤单录入请求
		void ReqParkedOrderAction(CThostFtdcParkedOrderActionField *pParkedOrderAction);
		///请求查询合约保证金率
		//void ReqQryInstrumentMarginRate();
		///请求查询合约手续费率
		//void ReqQryInstrumentCommissionRate();
		// 是否收到成功的响应
		bool IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo);

		template<typename message_t>
		bool is_own_message(const message_t& msg)
		{
			return msg.FrontID == front_id_ && msg.SessionID == session_id_;
		}

		static order_event_type::E get_order_event_type(char submit_status, char order_status)
		{
			LOGGER << "order_entry::get_order_event_type: " << submit_status << " " << order_status;

			switch(submit_status)
			{
				case THOST_FTDC_OSS_Accepted:
				{
					if(order_status == THOST_FTDC_OST_Canceled)
					{
						return order_event_type::cxl_accepted;
					}
					else
					{
						return order_event_type::accepted;
					}
				}
				case THOST_FTDC_OSS_InsertRejected:
				{
					return order_event_type::rejected;
				}
				case THOST_FTDC_OSS_CancelRejected:
				{
					return order_event_type::cxl_rejected;
				}
				default:
				{
					return order_event_type::unknown;
				}
			}
		}

		static position_direction::E get_order_direction(TThostFtdcDirectionType c)
		{
			switch(c)
			{
			case THOST_FTDC_D_Buy:
				return position_direction::buy;
			case THOST_FTDC_D_Sell:
					return position_direction::sell;
			default:
				return position_direction::unknown;
			}
		}

		static offset_flag::E get_offset_flag(char c)
		{
			switch(c)
			{
			case THOST_FTDC_OF_Open:
				return offset_flag::open;
			case THOST_FTDC_OF_Close:
				return offset_flag::close;
			default:
				return offset_flag::unknown;
			}
		}

	private:

		CThostFtdcTraderApi* trader_api_;

		strategy* strat_;

		std::string broker_id_;
		std::string investor_id_;
		std::string password_;

		int front_id_;
		int session_id_;
		int order_ref_;

		int request_id_;

		boost::mutex cv_mutex_;
		boost::condition_variable connected_cv_;
		boost::condition_variable login_cv_;
	};
}

#endif
