#!/bin/bash
sudo apt update
sudo apt install ninja-build build-essential libboost-all-dev libgtest-dev
mkdir -p build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ ..
ninja
