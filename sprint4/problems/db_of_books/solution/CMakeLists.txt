cmake_minimum_required(VERSION 3.11)

project(db_intro CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

#find_package(Boost 1.86.0 REQUIRED)
#if(Boost_FOUND)
#  include_directories(${Boost_INCLUDE_DIRS})
#endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Добавляем цели, указываем только их собственные файлы.
add_executable(book_manager
	src/main.cpp
	src/boost_json.cpp
	src/sdk.h
)
target_include_directories(book_manager PRIVATE CONAN_PKG::boost)
target_link_libraries(book_manager CONAN_PKG::boost CONAN_PKG::libpqxx)
