add_library(Messages STATIC
    src/MessageTypes/TextMessage.cpp
)

target_include_directories(Messages PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)