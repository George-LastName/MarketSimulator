cmake_minimum_required(VERSION 4.3.3)
project(NasdaqSimulator)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
)


add_subdirectory(exchange)
add_subdirectory(trader)
