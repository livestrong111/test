cmake_minimum_required(VERSION 3.24)
project(Order)

set(CMAKE_CXX_STANDARD 11)
link_libraries(mysqlclient)
link_libraries(jsoncpp)
link_libraries(muduo_base)
add_executable(Order order_system.hpp server.cpp easylogging++.cc)
