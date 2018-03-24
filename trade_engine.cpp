#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

#include "types.h"
#include "order_entry.h"
#include "market_data.h"
#include "strategy.h"
#include "logger.h"


int main(int argc, char** argv)
{	
	if(argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " $config_file" << std::endl;
		return -1;
	}

	if (!initialize_logger("trade_engine.log"))
	{
		return -1;
	}

	trade_engine::trade_config tc;
	if(!tc.read_from_file(argv[1]))
	{
		LOGGER << "Failed to read config file " << argv[1];
		return -1;
	}
	
	trade_engine::order_entry oe;
	if(!oe.initialize(tc))
	{
		LOGGER << "Failed to initialize order entry...";
		return -1;
	}
	

	trade_engine::strategy strat(oe);
	if(!strat.initialize(tc))
	{
		LOGGER << "Failed to initialize strategy...";
		return -1;
	}

	oe.set_strategy(strat);
	
	trade_engine::market_data md(strat);
	if(!md.initialize(tc))
	{
		LOGGER << "Failed to initialize market data...";
		return -1;
	}
    
    /*
	if(!oe.start())
	{
		LOGGER << "Failed to start order entry...";
		return -1;
	}
    */

	/*
	if(!strat.start())
	{
		std::cerr << "Failed to start strategy..." << std::endl;
		return -1;
	}
	*/
	

	if(!md.start())
	{
		LOGGER << "Failed to start market data...";
		return -1;
	}

	trade_engine::trade_config::md_sub_inst_list_t::iterator i = tc.md_inst.begin();
	for(;i != tc.md_inst.end(); ++i)
	{
		trade_engine::instrument* new_inst = trade_engine::instrument_factory::get_instance().create(i->c_str());
		if(new_inst)
		{
			md.subscribe_md(*new_inst);
		}
	}

	trade_engine::instrument_factory::get_instance().stop_create();
	
    
    /*
	int order_ref = 20;
	oe.send_order_insert_request("rb1610", '0', THOST_FTDC_D_Buy, 1,2100, order_ref, false);
	std::cout << "press enter to cancel..." << std::endl;
	char c = 0;
	std::cin >> c;
	oe.send_order_cancel_request("rb1610", order_ref);
    */

	boost::thread strat_thread(boost::bind(&trade_engine::strategy::process_loop, &strat));

	while(true)
	{	
		/*
		std::cout << "Input Q|q to exit:" << std::endl;
		char c = 0;
		std::cin >> c;
		if(c=='q' || c=='Q')
		{	
			strat.stop();
			break;
		}
		*/

		//global_sink()->stop();
		global_sink()->flush();
	}

	strat_thread.join();

	global_sink()->stop();
	global_sink()->flush();

    return 0;
}
