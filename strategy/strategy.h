#ifndef TRADE_ENGINE_STRATEGY_H
#define TRADE_ENGINE_STRATEGY_H

#include "types.h"
#include "./ctp_api/ThostFtdcUserApiStruct.h"
#include <boost/asio.hpp>
#include "logger.h"

namespace trade_engine
{
	class order_entry;

	class strategy
	{
	public:
		strategy(order_entry& oe) : oe_(oe), near_inst_(0), far_inst_(0),
			socket_(io_service_), current_active_combo_(0), stop_(false)
		{
		}

		bool initialize(const trade_config& config)
		{
			near_inst_ = instrument_factory::get_instance().create(config.near_inst.c_str());
			far_inst_ = instrument_factory::get_instance().create(config.far_inst.c_str());

			ip_ = config.strat_ip;
			port_ = config.strat_port;

			//TODO max_reject_times_ should read from GUI
			max_reject_times_ = 3;

			return true;
		}

		bool start()
		{
			//waiting for gui to connect
			using boost::asio::ip::tcp;
			boost::asio::ip::address_v4 addr;
			addr.from_string(ip_.c_str());
			tcp::acceptor acceptor(io_service_, tcp::endpoint(addr, atoi(port_.c_str())));
			//boost::asio::socket_base::non_blocking_io command(true);
			//acceptor.io_control(command);
			LOGGER << "Strategy Waiting for GUI to connect...";
			boost::system::error_code ec;
            while(true)
			{
				acceptor.accept(socket_, ec);
				if(!ec)
				{
					LOGGER << "Strategy got GUI connected from " << socket_.remote_endpoint().address() 
						   << ":" << socket_.remote_endpoint().port();
					boost::asio::socket_base::non_blocking_io command(true);
					socket_.io_control(command);
					break;
				}
				else
				{
					LOGGER << "strategy accept return error, accept again...";
				}
			}

			return true;
		}

		void deliver_md_update_event(const instrument* inst);

		void deliver_order_event(order_event& e);

		void handle_md_update_event(md_event& e);
		
		void handle_order_event(order_event& e);

		void process_loop()
		{
            BOOST_LOG_SCOPED_THREAD_TAG("ThreadID", boost::this_thread::get_id());

			while (true)
			{
				if (stop_)
				{
					break;
				}

				process();
			}
		}

		void process()
		{	
			const int max_md_event = 10;
			int md_event_processed = 0;
			while(++md_event_processed <= max_md_event)
			{
				md_event* e = md_event_queue_.top();
				if(e)
				{
					handle_md_update_event(*e);
					md_event_queue_.pop();
				}
				else
				{
					break;
				}
			}	

			process_gui_command();

			order_event* e = 0;
			while(e = order_event_queue_.top())
			{
				LOGGER << "strategy::process for order event queue: " << e->type;
				handle_order_event(*e);
				order_event_queue_.pop();
			}		
		}
	
		void stop()
		{
			stop_ = true;
		}

	private:
		void process_gui_command();

		void close_position(position& pos, uint32_t qty);

		struct strategy_priv;
		void on_combo_close(strategy_priv& combo);

	private:

		order_entry& oe_;

		instrument* near_inst_;
		instrument* far_inst_;

		int max_reject_times_;

		std::string ip_;
		std::string port_;

		spsc_event_queue<md_event, 1000> md_event_queue_;
		spsc_event_queue<order_event, 10000> order_event_queue_;

		boost::asio::io_service io_service_;
		boost::asio::ip::tcp::socket socket_;


		struct strategy_priv
		{
			strategy_priv() : status(position_status::initial), price_margin(0), reject_times(0) {}
			position near_pos;
			position far_pos;
			position_status::E status;
			double price_margin;
			int reject_times;
		};

		boost::unordered_map<int, strategy_priv*> orderref_2_priv_map_;
		strategy_priv* current_active_combo_;

		volatile bool stop_;
	};

}


#endif
