#ifndef TRADE_ENGINE_LOGGER_H
#define TRADE_ENGINE_LOGGER_H

#include <iostream>
#include <fstream>
#include <functional>

#include <boost/ref.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/record_ordering.hpp>

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords; 

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(global_logger, src::logger_mt)

#define LOGGER BOOST_LOG(global_logger::get())

typedef sinks::text_ostream_backend backend_t;

typedef sinks::asynchronous_sink<backend_t,
					sinks::bounded_ordering_queue<
								logging::attribute_value_ordering< unsigned int, std::less< unsigned int > >,
								1024, sinks::block_on_overflow>
> sink_t;

inline boost::shared_ptr<sink_t> global_sink()
{
	static boost::shared_ptr<sink_t> sink(new sink_t(
		boost::make_shared< backend_t >(),
		keywords::order = logging::make_attr_ordering("RecordID", std::less< unsigned int >())));
	return sink;
}

inline bool initialize_logger(const char* log_path)
{
	boost::shared_ptr< std::ostream > strm(new std::ofstream(log_path));
	if (!strm->good())
	{
		std::cerr << "Failed to open log file: " << log_path << std::endl;
		return false;
	}

	global_sink()->locked_backend()->add_stream(strm);

	global_sink()->set_formatter
		(
		expr::format("%1%: [%2%] [%3%] - %4%")
		% expr::attr< unsigned int >("RecordID")
		% expr::attr< boost::posix_time::ptime >("TimeStamp")
		% expr::attr< boost::thread::id >("ThreadID")
		% expr::smessage
		);

	// Add it to the core
	logging::core::get()->add_sink(global_sink());

	// Add some attributes too
	logging::core::get()->add_global_attribute("TimeStamp", attrs::local_clock());
	logging::core::get()->add_global_attribute("RecordID", attrs::counter< unsigned int >());

	LOGGER << "Succeed to initialize logger...";

	return true;
}

#endif
