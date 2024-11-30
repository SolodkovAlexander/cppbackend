cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Boost 1.86.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(Boost REQUIRED COMPONENTS url)

# Добавляем библиотеки
add_library(model_lib STATIC 
	src/model.h
    src/model.cpp
    src/tagged.h
)
add_library(loot_generator_lib STATIC 
	src/loot_generator.h
	src/loot_generator.cpp
)
add_library(json_logger_lib STATIC 
	src/json_logger.h
	src/json_logger.cpp
)
target_include_directories(json_logger_lib PRIVATE CONAN_PKG::boost)
target_link_libraries(json_logger_lib CONAN_PKG::boost)

add_library(json_parser_lib STATIC 
	src/json_parser.h
	src/json_parser.cpp
)
target_include_directories(json_parser_lib PRIVATE CONAN_PKG::boost)
target_link_libraries(json_parser_lib CONAN_PKG::boost model_lib)

add_library(http_server_lib STATIC 
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
)
target_include_directories(http_server_lib PRIVATE CONAN_PKG::boost)
target_link_libraries(json_parser_lib CONAN_PKG::boost json_logger_lib)

add_library(collision_detector_lib STATIC 
	src/collision_detector.cpp
	src/collision_detector.h
	src/geom.h
)

# Добавляем цели, указываем только их собственные файлы.
add_executable(game_server
	src/main.cpp
	src/boost_json.cpp
	src/request_handler.cpp
	src/request_handler.h
	src/ticker.h
	src/players.h
	src/players.cpp
	src/application.h
	src/application.cpp
	src/server_state_saver.h
	src/server_state_saver.cpp
	src/database_controller.h
)
target_include_directories(game_server PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server 
	CONAN_PKG::boost
	CONAN_PKG::libpqxx
	model_lib 
	loot_generator_lib
	json_logger_lib
	json_parser_lib
	http_server_lib
	collision_detector_lib	
)

add_executable(game_server_tests
    tests/model-tests.cpp
    tests/loot_generator_tests.cpp
    tests/collision-detector-tests.cpp
)
target_include_directories(game_server_tests PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server_tests PRIVATE 
	CONAN_PKG::catch2 
	CONAN_PKG::boost 
	model_lib 
	loot_generator_lib
	collision_detector_lib
) 
