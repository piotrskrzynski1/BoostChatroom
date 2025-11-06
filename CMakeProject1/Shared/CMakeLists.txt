
cmake_minimum_required(VERSION 3.16)

find_package(Boost REQUIRED COMPONENTS system filesystem)

file(GLOB_RECURSE MESSAGE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_library(Messages STATIC ${MESSAGE_SOURCES}
        include/MessageTypes/Utilities/HeaderHelper.h
        src/MessageTypes/File/FileMessage.cpp
        include/MessageTypes/File/FileMessage.h
        src/MessageTypes/Utilities/MessageFactory.cpp
        include/MessageTypes/Utilities/MessageFactory.h)

target_include_directories(Messages PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(Messages PRIVATE ${Boost_INCLUDE_DIRS})

message(STATUS "Messages library sources: ${MESSAGE_SOURCES}")
