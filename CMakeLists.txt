cmake_minimum_required(VERSION 3.16)

project(mmkv_c
    VERSION 2.4.0
    DESCRIPTION "Static C facade for Tencent MMKV"
    LANGUAGES C CXX ASM)

option(MMKV_C_BUILD_TESTS "Build mmkv_c smoke tests" OFF)
option(MMKV_C_FORCE_POSIX "Use MMKV's POSIX implementation on Apple and Android" ON)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

add_subdirectory(MMKV/Core mmkv_core)

if(MMKV_C_FORCE_POSIX AND (APPLE OR ANDROID))
    target_compile_definitions(core PUBLIC FORCE_POSIX)
endif()

add_library(mmkv_c STATIC
    src/mmkv_c.cpp)

add_library(mmkv_c::mmkv_c ALIAS mmkv_c)

target_include_directories(mmkv_c
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(mmkv_c
    PUBLIC
        core)

target_compile_features(mmkv_c PRIVATE cxx_std_20)
set_target_properties(mmkv_c PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME mmkv_c)

install(TARGETS mmkv_c core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)

install(DIRECTORY include/ DESTINATION include)

if(MMKV_C_BUILD_TESTS)
    enable_testing()
    add_executable(mmkv_c_header_c tests/header_c.c)
    target_link_libraries(mmkv_c_header_c PRIVATE mmkv_c)
    add_test(NAME mmkv_c_header_c COMMAND mmkv_c_header_c)

    add_executable(mmkv_c_smoke tests/smoke.cpp)
    target_link_libraries(mmkv_c_smoke PRIVATE mmkv_c)
    target_compile_features(mmkv_c_smoke PRIVATE cxx_std_20)
    add_test(NAME mmkv_c_smoke COMMAND mmkv_c_smoke)
endif()
