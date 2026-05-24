cmake_minimum_required(VERSION 4.3.3)
project (Nasdaq_Exchange)

add_executable(${PROJECT_NAME}
    exchange_setup.cpp
    src/exchange.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)


