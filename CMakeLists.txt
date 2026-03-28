cmake_minimum_required(VERSION 3.16.0)
# Fix: mbedtls find_package(Threads) try_compile fails on Windows with xtensa cross-compiler.
# STATIC_LIBRARY mode skips the executable link step that breaks cross-compilation detection.
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ECO_Voice)
