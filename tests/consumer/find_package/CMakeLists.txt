# Consumes an installed hh: cmake -S . -B build -DCMAKE_PREFIX_PATH=<install prefix>
cmake_minimum_required(VERSION 3.16)
project(hh_consumer_find_package LANGUAGES CXX)

find_package(hh 1.0 REQUIRED)

add_executable(consumer ../consumer.cpp)
target_link_libraries(consumer PRIVATE hh::hh)

enable_testing()
add_test(NAME consumer COMMAND consumer)
