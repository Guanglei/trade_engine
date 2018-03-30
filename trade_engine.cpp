#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

#include "types.h"
#include "order_entry.h"
#include "market_data.h"
#include "strategy/strategy.h"
#include "logger.h"

bool stop_flush_log = false;
void flush_log()
{
    while(!stop_flush_log)
    {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(500)); 
	    global_sink()->flush();
    }
}

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

    BOOST_LOG_SCOPED_THREAD_TAG("ThreadID", boost::this_thread::get_id());

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
    
	if(!oe.start())
	{
		LOGGER << "Failed to start order entry...";
		return -1;
	}

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
            LOGGER << "create a new instrument id [" << new_inst->id << "] internal_id [" << new_inst->internal_id << "]";
			md.subscribe_md(*new_inst);
		}
	}

	trade_engine::instrument_factory::get_instance().stop_create();
	
    
	int order_ref = 0;
    std::string symbol;
    char side = 'S';
    char tif = 'I';
    uint32_t qty = 0;
    int32_t price = 0;
    char pos_offset = 'O';

	boost::thread strat_thread(boost::bind(&trade_engine::strategy::process_loop, &strat));

	boost::thread log_flush_thread(boost::bind(flush_log));
   
    bool quit = false;
	while(!quit)
	{	
        std::cout << "Input 'N $symbol $side(B|S) $tif(D|I) $qty $price $pos' to enter new limit order" << std::endl;
        std::cout << "Input 'M $symbol $side(B|S) $qty $pos' to enter new market order" << std::endl;
        std::cout << "Input 'C $symbol $order_ref'" << std::endl;
		std::cout << "Input 'Q' to exit:" << std::endl;
		char c = 0;
		std::cin >> c;
        switch(c)
        {
            case 'N':
                {
                    std::cin >> symbol >> side >> tif >> qty >> price >> pos_offset;
                    oe.send_order_insert_request(symbol, pos_offset == 'O' ? THOST_FTDC_OF_Open : THOST_FTDC_OF_CloseToday, 
                                side == 'B' ? THOST_FTDC_D_Buy : THOST_FTDC_D_Sell, qty, price, order_ref, tif=='I');
                    break;
                }
            case 'M':
                {
                    std::cin >> symbol >> side >> qty >> pos_offset;
                    oe.send_order_insert_request(symbol, pos_offset == 'O' ? THOST_FTDC_OF_Open : THOST_FTDC_OF_CloseToday, side == 'B' ? THOST_FTDC_D_Buy : THOST_FTDC_D_Sell, qty, order_ref);
                    break;
                }
            case 'C':
                {
                    int oid = 0;
                    std::cin >> symbol >> oid;
                    oe.send_order_cancel_request(symbol, oid);
                    break;
                }
            case 'Q':
                {

			        strat.stop();
                    stop_flush_log = true;
                    quit = true;
			        break;
                }
        }
	}

	strat_thread.join();
    log_flush_thread.join();

	global_sink()->stop();
	global_sink()->flush();

    return 0;
}
