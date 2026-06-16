cmake_minimum_required(VERSION 3.16)
project (Nasdaq_Trader)

add_executable(${PROJECT_NAME}
    trader_setup.cpp
    src/trader.cpp
    src/order_book.cpp
    src/soup_bin_tcp_client.cpp
    src/ouch_client.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)


if(LOCAL_TESTING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LOCAL_TESTING=1)
endif()
