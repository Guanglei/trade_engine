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

		///���ͻ����뽻�׺�̨ͨ�����ӶϿ�ʱ���÷��������á���������������API���Զ��������ӣ��ͻ��˿ɲ�������
		///@param nReason ����ԭ��
		///        0x1001 �����ʧ��
		///        0x1002 ����дʧ��
		///        0x2001 ����������ʱ
		///        0x2002 ��������ʧ��
		///        0x2003 �յ�������
		virtual void OnFrontDisconnected(int nReason);
		
		///������ʱ���档����ʱ��δ�յ�����ʱ���÷��������á�
		///@param nTimeLapse �����ϴν��ձ��ĵ�ʱ��
		virtual void OnHeartBeatWarning(int nTimeLapse);

		///���ͻ����뽻�׺�̨������ͨ������ʱ����δ��¼ǰ�����÷��������á�
		virtual void OnFrontConnected();
	
		///��¼������Ӧ
		virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///��������Ӧ��
		virtual void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///ȡ����������Ӧ��
		virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

		///�������֪ͨ
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
