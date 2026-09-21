# Consumes hh from its source tree, as a git submodule would be used.
cmake_minimum_required(VERSION 3.16)
project(hh_consumer_add_subdirectory LANGUAGES CXX)

add_subdirectory(../../.. hh)

# As a subproject hh builds the library only.
if(TARGET hh_tests OR TARGET hh_cli)
    message(FATAL_ERROR "hh must not build its tests or examples as a subproject")
endif()

add_executable(consumer ../consumer.cpp)
target_link_libraries(consumer PRIVATE hh::hh)

enable_testing()
add_test(NAME consumer COMMAND consumer)
