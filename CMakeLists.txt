cmake_minimum_required(VERSION 3.14)

project(jpeg-decoder)

include(FetchContent)
include(cmake/FindGlog.cmake)

find_package(FFTW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -pedantic -Werror)

add_subdirectory(jpeg-decoder-lib)
