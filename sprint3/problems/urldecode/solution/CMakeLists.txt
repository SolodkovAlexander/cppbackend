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

add_executable(urldecode
    src/main.cpp
    src/urldecode.h
    src/urldecode.cpp
)
target_include_directories(urldecode PRIVATE CONAN_PKG::boost)
target_link_libraries(urldecode CONAN_PKG::boost) 

add_executable(tests
    tests/tests.cpp
    src/urldecode.h
    src/urldecode.cpp
)
target_link_libraries(tests PRIVATE CONAN_PKG::boost)
