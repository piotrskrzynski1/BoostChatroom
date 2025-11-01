
cmake_minimum_required(VERSION 3.16)

find_package(Boost REQUIRED COMPONENTS asio)

file(GLOB_RECURSE MESSAGE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_library(Messages STATIC ${MESSAGE_SOURCES})

target_include_directories(Messages PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Messages PUBLIC Boost::asio)

message(STATUS "Messages library sources: ${MESSAGE_SOURCES}")
