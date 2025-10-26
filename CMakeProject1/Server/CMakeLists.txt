# CMakeList.txt: projekt CMake dla elementu CMakeProject1, dołącz źródło i zdefiniuj
# tutaj logikę specyficzną dla projektu.
#

# Dodaj źródło do pliku wykonywalnego tego projektu.
find_package(Boost REQUIRED COMPONENTS asio)

set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

file(GLOB_RECURSE SOURCES
	${SOURCE_DIR}/*.cpp
	${SOURCE_DIR}/*.c
)
file(GLOB_RECURSE HEADERS
	${HEADER_DIR}/*.h
	${HEADER_DIR}/*.hpp
)

add_executable (server ${SOURCES})

target_include_directories(server PRIVATE ${HEADER_DIR})


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET server PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(server PRIVATE Messages)
target_link_libraries(server PRIVATE Boost::asio)

# TODO: Dodaj testy i zainstaluj elementy docelowe w razie potrzeby.
