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
add_library(ModelLib STATIC 
	src/model.h
    src/model.cpp
    src/tagged.h
)
add_library(LootGeneratorLib STATIC 
	src/loot_generator.h
	src/loot_generator.cpp
)
add_library(JsonLoggerLib STATIC 
	src/json_logger.h
	src/json_logger.cpp
)
target_include_directories(JsonLoggerLib PRIVATE CONAN_PKG::boost)
target_link_libraries(JsonLoggerLib CONAN_PKG::boost)

add_library(JsonParserLib STATIC 
	src/json_parser.h
	src/json_parser.cpp
)
target_include_directories(JsonParserLib PRIVATE CONAN_PKG::boost)
target_link_libraries(JsonParserLib CONAN_PKG::boost ModelLib)

add_library(HttpServerLib STATIC 
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
)
target_include_directories(HttpServerLib PRIVATE CONAN_PKG::boost)
target_link_libraries(JsonParserLib CONAN_PKG::boost JsonLoggerLib)

add_library(CollisionDetectorLib STATIC 
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
	src/application.h
	src/players.h
)
target_include_directories(game_server PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server CONAN_PKG::boost 
	ModelLib 
	LootGeneratorLib
	JsonLoggerLib
	JsonParserLib
	HttpServerLib
	CollisionDetectorLib
)

add_executable(game_server_tests
    tests/model-tests.cpp
    tests/loot_generator_tests.cpp
    tests/collision-detector-tests.cpp
)
target_include_directories(game_server_tests PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server_tests PRIVATE 
	CONAN_PKG::catch2 
	CONAN_PKG::boost ModelLib 
	LootGeneratorLib
	CollisionDetectorLib
) 