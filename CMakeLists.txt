cmake_minimum_required(VERSION 3.24)
project(async-stream VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COLOR_DIAGNOSTICS ON)

find_package(fmt CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(tl-expected CONFIG REQUIRED)

add_executable(main source/main.cpp)
target_include_directories(main PRIVATE source)
target_link_libraries(main PRIVATE fmt::fmt asio::asio tl::expected)
