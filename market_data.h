#ifndef TRADE_ENGINE_MARKET_DATA_H
#define TRADE_ENGINE_MARKET_DATA_H

#include "./ctp_api/ThostFtdcMdApi.h"
#include "types.h"

#include <boost/thread/mutex.hpp>  
#include <boost/thread/condition.hpp>
#include <vector>
#include <string.h>
#include "logger.h"

namespace trade_engine
{
	class strategy;

	class market_data : public CThostFtdcMdSpi
	{
	public:

		market_data(strategy& strat) : strat_(strat), md_api_(0), request_id_(0) {}

		bool initialize(const trade_config& config)
		{
			broker_id_ = config.broker_id;
			investor_id_ = config.investor_id;
			password_ = config.password;

			md_api_ = CThostFtdcMdApi::CreateFtdcMdApi();
			if(!md_api_)
			{
				LOGGER << "Failed to CreateFtdcMdApi...";
				return false;
			}
			md_api_->RegisterSpi(this);

			std::string front_addr = "tcp://" + config.md_ip + ":" + config.md_port;
			LOGGER << "Market data front address: " << front_addr;
			md_api_->RegisterFront(const_cast<char*>(front_addr.c_str()));	
			return true;
		}

		bool start();

	public:
		virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo,
										int nRequestID, bool bIsLast);

		///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
		///@param nReason 错误原因
		///        0x1001 网络读失败
		///        0x1002 网络写失败
		///        0x2001 接收心跳超时
		///        0x2002 发送心跳失败
		///        0x2003 收到错误报文
		virtual void OnFrontDisconnected(int nReason);
		
		///心跳超时警告。当长时间未收到报文时，该方法被调用。
		///@param nTimeLapse 距离上次接收报文的时间
		virtual void OnHeartBeatWarning(int nTimeLapse);

		///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
		virtual void OnFrontConnected();
	
		///登录请求响应
		virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///订阅行情应答
		virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///取消订阅行情应答
		virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///深度行情通知
		virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData);

		void subscribe_md(instrument&);

	private:
		bool IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo);

	private:

		strategy& strat_;

		CThostFtdcMdApi* md_api_;
		int request_id_;

		std::string broker_id_;
		std::string investor_id_;
		std::string password_;

		boost::mutex cv_mutex_;
		boost::condition_variable connected_cv_;
		boost::condition_variable login_cv_;
	};
}

#endif
