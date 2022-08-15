#!/bin/bash

TYPE="${1:=Release}"
echo $TYPE

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
   export CXXFLAGS="-stdlib=libc++"
   export CC=/usr/bin/clang
   export CXX=/usr/bin/clang++ 
	CMAKE_CMD="cmake -DCMAKE_BUILD_TYPE=$TYPE .."
elif [[ "$OSTYPE" == "darwin"* ]]; then
	CMAKE_CMD="cmake -DCMAKE_BUILD_TYPE=$TYPE .."
else
   echo Unsupported platform!
   exit 1
fi

set -ex

rm -rf build
mkdir -p build
cd build
set -ex

$CMAKE_CMD
cmake --build . -j 8
cd ..

ls build/
