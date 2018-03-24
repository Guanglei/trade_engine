#ifndef TRADE_DEMO_TYPES_H
#define TRADE_DEMO_TYPES_H

#include <boost/unordered_map.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/unordered_set.hpp>
#include <boost/atomic.hpp>
#include <exception>
#include <iostream>
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "./ctp_api/ThostFtdcUserApiDataType.h"
#include "util.h"

namespace trade_engine
{
	struct top_of_book
	{
		double bid_price;
		uint32_t bid_qty;
		double ask_price;
		uint32_t ask_qty;
		double last_price;

		top_of_book() : bid_price(0), bid_qty(0),
						ask_price(0), ask_qty(0), last_price(0)
		{
		}

		top_of_book(const top_of_book& tob) : bid_price(tob.bid_price),
											  bid_qty(tob.bid_qty),
											  ask_price(tob.ask_price),
											  ask_qty(tob.ask_qty),
											  last_price(tob.last_price)
		{
		}

		top_of_book& operator=(const top_of_book& tob)
		{
			bid_price = tob.bid_price;
			bid_qty = tob.bid_qty;
			ask_price = tob.ask_price;
			ask_qty = tob.ask_qty;
			last_price = tob.last_price;
			return *this;
		}
	};

	struct md_event : top_of_book
	{
		md_event() : internal_inst_id(0)
		{
		}

		md_event(const md_event& e) : top_of_book(e), internal_inst_id(e.internal_inst_id)
		{
		}

		md_event& operator=(const md_event& e)
		{
			static_cast<top_of_book&>(*this) = e;
			internal_inst_id = e.internal_inst_id;
			return *this;
		}

		int internal_inst_id;
	};

	struct order_event_type
	{
		enum E
		{
			accepted,
			rejected,
			cxl_accepted,
			cxl_rejected,
			fill,
			unknown
		};
	};

	struct instrument;

	struct position_direction
	{
		enum E
		{
			buy,
			sell,
			unknown
		};
	};

	struct offset_flag
	{
		enum E
		{
			open,
			close,
			unknown
		};
	};

	struct order_event
	{
		int order_ref;
		order_event_type::E type;
		double	price;
		uint32_t quantity;
		position_direction::E direction;
		offset_flag::E offset;
		instrument* inst;

		order_event& operator=(const order_event& e)
		{
			order_ref = e.order_ref;
			type = e.type;
			price = e.price;
			quantity = e.quantity;
			direction = e.direction;
			offset = e.offset;
			inst = e.inst;
			return *this;
		}
	};
	
	struct position_status
	{
		enum E
		{
			initial,
			pending_open,
			open,
			pending_close,
			close
		};
	};

	struct position
	{
		position() : status(position_status::initial), direction(position_direction::unknown),
					 price(0.0), quantity(0), open_trade_price(0.0), open_trade_quantity(0), 
					 close_trade_price(0.0), close_trade_quantity(0), open_balance(0.0), close_balance(0.0), inst(0),
					 open_order_ref(0), close_order_ref(0)
		{
		}

		position_status::E status;
		position_direction::E direction;
		double	price;
		uint32_t quantity;
		double open_trade_price;
		uint32_t open_trade_quantity;
		double close_trade_price;
		uint32_t close_trade_quantity;
		double open_balance;
		double close_balance;
		instrument* inst;
		int open_order_ref;
		int close_order_ref;
	};

	struct instrument
	{
		instrument() : internal_id(0)
		{
			memset(id, 0, sizeof(TThostFtdcInstrumentIDType));
		}

		int internal_id;
		TThostFtdcInstrumentIDType id;
		top_of_book tob;
	};

	class instrument_factory
	{
	public:

		static instrument_factory& get_instance()
		{
			static instrument_factory me;
			return me;
		}

		instrument* create(const char* inst_id)
		{
			unsigned int hash_id = APHash(inst_id);
			boost::unordered_map<unsigned int, instrument*>::iterator i = created_inst_.find(hash_id);
			if(i != created_inst_.end())
			{
				return i->second;
			}

			if(!stop_create_)
			{
				instrument* new_inst = new instrument;
				new_inst->internal_id = hash_id;
				strcpy(new_inst->id, inst_id);
				created_inst_[hash_id] = new_inst;
				return new_inst;
			}
			else
			{
				return 0;
			}
		}

		void stop_create()
		{
			stop_create_ = true;
		}

		void destory(instrument* inst)
		{
			delete inst;
		}

	private:
		instrument_factory() : stop_create_(false)
		{
		}

		instrument_factory(const instrument_factory&);
		instrument_factory& operator=(const instrument_factory&);

	private:
		bool stop_create_;
		boost::unordered_map<unsigned int, instrument*> created_inst_;
	};


	struct trade_config
	{
		std::string trd_ip;
		std::string trd_port;
		std::string md_ip;
		std::string md_port;
		std::string broker_id;
		std::string investor_id;
		std::string password;

		typedef std::vector<std::string> md_sub_inst_list_t;
		md_sub_inst_list_t md_inst;

		std::string near_inst;
		std::string far_inst;
		std::string strat_ip;
		std::string strat_port;

		bool read_from_file(const std::string& file)
		{
			using boost::property_tree::ptree;
			ptree pt;
			ptree root;
			try
			{
				read_xml(file.c_str(), pt);
				root = pt.get_child("config");
			}
			catch(std::exception& e)
			{
				std::cerr << "Failed to read config file: " << e.what() << std::endl;
				return false;
			}
		
			try
			{
				for(ptree::iterator itr = root.begin(); 
									itr!=root.end();itr++)
				{
					if(itr->first == "broker-id")
					{
						broker_id = itr->second.data();
					}
					else if(itr->first == "investor-id")
					{
						investor_id = itr->second.data();
					}
					else if(itr->first == "password")
					{
						password = itr->second.data();
					}
					else if(itr->first == "order-entry")
					{	
						ptree element = itr->second;
						trd_ip = element.get_child("ip").data();
						trd_port = element.get_child("port").data();
					}
					else if(itr->first == "market-data")
					{
						ptree element = itr->second;
						md_ip = element.get_child("ip").data();
						md_port = element.get_child("port").data();
						element = element.get_child("subscription");
						for(ptree::iterator itr2 = element.begin(); 
											itr2!=element.end(); ++itr2)
						{
							if(itr2->first == "inst")
							{
								md_inst.push_back(std::string(itr2->second.data()));
							}
						}
					}
					else if(itr->first == "strategy")
					{
						ptree element = itr->second;
						near_inst = element.get_child("near-inst").data();
						far_inst = element.get_child("far-inst").data();
						strat_ip = element.get_child("ip").data();
						strat_port = element.get_child("port").data();
					}
				}
			}
			catch(...)
			{
				std::cerr << "exception thrown when parse xml..." << std::endl;
				return false;
			}

			return true;
		}
	};

	//only support one producer thread and on consumer thread
	template<typename event_t, size_t event_queue_size>
	class spsc_event_queue
	{
	public:
		spsc_event_queue() : producer_cur_(0), consumer_cur_(0)
		{
		}

		//producer call enqueue
		bool enqueue(const event_t& e)
		{
			uint64_t p_cur = producer_cur_.load(boost::memory_order_relaxed);
			uint64_t c_cur = consumer_cur_.load(boost::memory_order_acquire);
			uint64_t diff = p_cur - c_cur;
			if(diff < event_queue_size)
			{
				buf_[p_cur % event_queue_size] = e;
				producer_cur_.fetch_add(1, boost::memory_order_release);
				return true;
			}

			return false;
		}

		//consumer call top
		event_t* top()
		{
			uint64_t p_cur = producer_cur_.load(boost::memory_order_acquire);
			uint64_t c_cur = consumer_cur_.load(boost::memory_order_relaxed);
			return c_cur < p_cur ? &buf_[c_cur % event_queue_size] : 0;
		}

		//consumer call pop
		bool pop()
		{
			uint64_t p_cur = producer_cur_.load(boost::memory_order_acquire);
			uint64_t c_cur = consumer_cur_.load(boost::memory_order_relaxed);
			if(c_cur < p_cur)
			{
				consumer_cur_.fetch_add(1, boost::memory_order_relaxed);
				return true;
			}

			return false;
		}

	private:
		event_t buf_[event_queue_size];
		boost::atomic<uint64_t> producer_cur_;
		boost::atomic<uint64_t> consumer_cur_;
	};


	//gui communication struct
	struct gui_command
	{
			unsigned int CommandType; //0表示初始化合约 1表示开平仓命令 2表示撤单命令
			TThostFtdcInstrumentIDType	InstrumentID[2];//合约
			TThostFtdcPriceType PriceMargin;
			TThostFtdcVolumeType	VolumeTotalOriginal;
			TThostFtdcOffsetFlagType	CombOffsetFlag; 	//组合开平标志
			bool	CancelOrder; //撤单命令
	};
	
	struct instrument_info
	{
		TThostFtdcInstrumentIDType	InstrumentID;//合约代码
		TThostFtdcPriceType	LastPrice; //最新价
		TThostFtdcDateType	TradingDay; //交易日
		TThostFtdcTimeType	UpdateTime;///最后修改时间
		TThostFtdcMillisecType	UpdateMillisec;///最后修改毫秒
	};
}
#endif
