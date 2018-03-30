cmake_minimum_required(VERSION 2.8.9)

project (ctp_trading_engine)

file(GLOB STRAT_SRC ./strategy/*.cpp)
set (TRADING_ENGINE_SRC market_data.cpp order_entry.cpp ${STRAT_SRC} trade_engine.cpp types.cpp)
add_executable(trading_engine ${TRADING_ENGINE_SRC})

include_directories(.)

target_link_libraries(trading_engine "${CMAKE_SOURCE_DIR}/ctp_api/thostmduserapi.so"
    "${CMAKE_SOURCE_DIR}/ctp_api/thosttraderapi.so" pthread boost_log boost_thread boost_system)

add_definitions(-DBOOST_LOG_DYN_LINK)
