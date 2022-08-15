#!/bin/bash

TYPE="Release"
echo $TYPE

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
	sudo apt-get install -y make git zlib1g-dev libssl-dev gperf php-cli cmake clang libc++-dev libc++abi-dev
    export CXXFLAGS="-stdlib=libc++"
    export CC=/usr/bin/clang
    export CXX=/usr/bin/clang++ 
    CMAKE_CMD="cmake -DCMAKE_BUILD_TYPE=$TYPE -DCMAKE_INSTALL_PREFIX:PATH=../tdlib .."
elif [[ "$OSTYPE" == "darwin"* ]]; then
	brew install gperf cmake openssl
    CMAKE_CMD="cmake -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl/ -DCMAKE_INSTALL_PREFIX:PATH=../tdlib .."
else
   echo Unsupported platform!
   exit 1
fi

git submodule add https://github.com/tdlib/td.git
git submodule update

cd td
git checkout master

rm -rf build && mkdir -p build && cd build

set -ex
$CMAKE_CMD
cmake --build . --target install -j 8
cd ../..
