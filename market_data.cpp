#include "market_data.h"
#include "strategy/strategy.h"
#include <iostream>
#include <boost/thread.hpp>
#include "util.h"

namespace trade_engine
{
	bool market_data::start()
	{
		md_api_->Init();

		{
			boost::mutex::scoped_lock lock(cv_mutex_);
			if(!connected_cv_.timed_wait(lock, boost::get_system_time() + boost::posix_time::seconds(25)))
			{
				LOGGER << "MD: 25 seconds passed but still not connected, failed the start...";
				return false;
			}
		}

		LOGGER << "MD connected successfully, going to send login request...";

		CThostFtdcReqUserLoginField req;
		memset(&req, 0, sizeof(req));
		strcpy(req.BrokerID, broker_id_.c_str());
		strcpy(req.UserID, investor_id_.c_str());
		strcpy(req.Password, password_.c_str());
		int ret = md_api_->ReqUserLogin(&req, ++request_id_);
		if(ret != 0)
		{
			LOGGER << "MD failed to send login request...";
			return false;
		}

		{
			boost::mutex::scoped_lock lock(cv_mutex_);
			if(!login_cv_.timed_wait(lock, boost::get_system_time() + boost::posix_time::seconds(25)))
			{
				LOGGER << "MD: 25 seconds passed but still not login, failed the start...";
				return false;
			}
		}

		LOGGER << "MD Login successfully...";

		return true;
	}

	void market_data::OnRspError(CThostFtdcRspInfoField *pRspInfo,
										int nRequestID, bool bIsLast)
	{
		IsErrorRspInfo(pRspInfo);
	}

	void market_data::OnFrontDisconnected(int nReason)
	{
		LOGGER << "market_data::OnFrontDisconnected Reason = " << nReason;
	}
		
	void market_data::OnHeartBeatWarning(int nTimeLapse)
	{
		LOGGER << "market_data::OnHeartBeatWarning nTimerLapse = " << nTimeLapse;
	}

	void market_data::OnFrontConnected()
	{	
        BOOST_LOG_SCOPED_THREAD_TAG("ThreadID", boost::this_thread::get_id());
		LOGGER << "market_data::OnFrontConnected...";
		connected_cv_.notify_all();
	}

	void market_data::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
										CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{
		if(IsErrorRspInfo(pRspInfo))
		{
			return;
		}
		
		login_cv_.notify_all();
	}

	void market_data::subscribe_md(instrument& inst)
	{
		char* insts[] = {inst.id};
		if(md_api_->SubscribeMarketData(insts, 1) == 0)
		{
			LOGGER << "Succeed to send md subscription request for instrument " << inst.id;
			
		}
		else
		{
			LOGGER << "Failed to send md subscription request for instrument " << inst.id;
		}
	}

	void market_data::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, 
											CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{
		IsErrorRspInfo(pRspInfo);
		LOGGER << "Received response for market data subscription for instrument " << pSpecificInstrument->InstrumentID;
	}
	
	void market_data::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, 
											CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
	{
		LOGGER << "Unsubscribe md for inst: " << pSpecificInstrument->InstrumentID;
	}

	void market_data::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
	{
		instrument* inst = instrument_factory::get_instance().create(pDepthMarketData->InstrumentID);
		if(inst)
		{
			inst->tob.bid_price = pDepthMarketData->BidPrice1;
			inst->tob.bid_qty = pDepthMarketData->BidVolume1;
			inst->tob.ask_price = pDepthMarketData->AskPrice1;
			inst->tob.ask_qty = pDepthMarketData->AskVolume1;
			inst->tob.last_price = pDepthMarketData->LastPrice;
			strat_.deliver_md_update_event(inst);
			
		}
	}

	bool market_data::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
	{
		bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
		if (bResult)
			LOGGER << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg;
		return bResult;
	}
}
