cmake_minimum_required(VERSION 3.16)

project(mmkv_c_test_package LANGUAGES CXX)

find_package(mmkv_c CONFIG REQUIRED)

add_executable(mmkv_c_test_package test_package.cpp)
target_link_libraries(mmkv_c_test_package PRIVATE mmkv_c::mmkv_c)
target_compile_features(mmkv_c_test_package PRIVATE cxx_std_17)
