# CMakeList.txt: plik projektu CMake najwyższego poziomu, wykonaj konfigurację globalną
# i uwzględnij podprojekty w tym miejscu.
cmake_minimum_required (VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak -g")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory -g")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(
          -Wall           # Base warnings
          -Wextra         # Extra warnings
          -Wpedantic      # Enforce strict standard compliance
          -Wconversion    # Warn implicit type conversions (optional but useful)
          -Wshadow        # Warn shadowed variables (very useful)
          -fdiagnostics-color=always  # Colored output in terminal
  )
endif()

project ("ChatApp")


# Uwzględnij podprojekty.
add_subdirectory("CMakeProject1/Shared")
add_subdirectory("CMakeProject1/Client")
add_subdirectory("CMakeProject1/Server")
add_subdirectory("CMakeProject1/Tests")

#sudo apt update
#sudo apt install ninja-build build-essential libboost-all-dev libgtest-dev

#mkdir -p build
#cd build
#cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ ..
#ninja
