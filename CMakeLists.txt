cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(mem_jit CXX)

set(ASMJIT_STATIC TRUE)
add_definitions(-DASMJIT_STATIC)

add_subdirectory(vendor EXCLUDE_FROM_ALL)

add_library(${PROJECT_NAME} STATIC
    src/jit_scanner.cpp
    include/mem/jit_scanner.h)

target_include_directories(${PROJECT_NAME}
    PUBLIC include
    PRIVATE vendor/asmjit/src)

target_link_libraries(${PROJECT_NAME}
    asmjit mem)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON)
