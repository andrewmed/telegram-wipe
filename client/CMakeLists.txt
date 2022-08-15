cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(client VERSION 1.0 LANGUAGES CXX)

include_directories (../td/tdlib/include) 

set(CMAKE_FIND_PACKAGE_RESOLVE_SYMLINKS, true)
find_package(Td REQUIRED PATHS ../td/tdlib/lib/cmake/Td)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE Td::TdStatic)
set_property(TARGET client PROPERTY CXX_STANDARD 17)
