# CMakeList.txt: projekt CMake dla elementu CMakeProject1, dołącz źródło i zdefiniuj
# tutaj logikę specyficzną dla projektu.
#
# Dodaj źródło do pliku wykonywalnego tego projektu.

find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(GTest REQUIRED)

set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(CLIENT_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../Client/include)

set(SERVER_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../Server/include)


file(GLOB_RECURSE SOURCES
	${SOURCE_DIR}/*.cpp
	${SOURCE_DIR}/*.c
)
file(GLOB_RECURSE HEADERS
	${HEADER_DIR}/*.h
	${HEADER_DIR}/*.hpp
)

file(GLOB_RECURSE SERVER_SOURCES
		../Server/src/*.cpp
)

file(GLOB_RECURSE CLIENT_SOURCES
		../Client/src/*.cpp
)

list(FILTER SERVER_SOURCES EXCLUDE REGEX "ServerMain.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "ClientMain.cpp$")

add_executable (tests ${SOURCES}
		include/ServerManagerTest.h
		${SERVER_SOURCES}
		${CLIENT_SOURCES}
)


target_include_directories(tests PRIVATE ${HEADER_DIR})
target_include_directories(tests PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(tests PRIVATE ${GTEST_INCLUDE_DIRS})
target_include_directories(tests PRIVATE
		# ... inne ścieżki (GTest, Boost, itd.)

		# Dodanie Client/include
		${CLIENT_INCLUDE_ROOT}

		# Dodanie Server/include
		${SERVER_INCLUDE_ROOT}
)


target_link_libraries(tests PRIVATE Messages)
target_link_libraries(tests PRIVATE GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(tests)

# TODO: Dodaj testy i zainstaluj elementy docelowe w razie potrzeby.
