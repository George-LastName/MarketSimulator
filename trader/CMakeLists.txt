cmake_minimum_required(VERSION 4.3.3)
project (Nasdaq_Trader)

add_executable(${PROJECT_NAME}
    trader_setup.cpp
    src/trader.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
