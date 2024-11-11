cmake_minimum_required(VERSION 3.11)

project(urldecode CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Boost 1.86.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

find_package(Boost REQUIRED COMPONENTS url)

add_executable(urlencode
    src/main.cpp
    src/urlencode.h
    src/urlencode.cpp
)
target_include_directories(urlencode PRIVATE CONAN_PKG::boost)
target_link_libraries(urlencode CONAN_PKG::boost) 

add_executable(tests
    tests/tests.cpp
    src/urlencode.h
    src/urlencode.cpp
)
target_link_libraries(tests PRIVATE CONAN_PKG::gtest CONAN_PKG::boost)
