cmake_minimum_required (VERSION 3.7.2)
project (astation)
include(FindPkgConfig)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# My stuff
add_executable(${PROJECT_NAME} src/main.cpp src/Video.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE "-Wextra" "-Werror" "-Wall" "-pedantic-errors" "-Wconversion")

# TCLAP
find_package(TCLAP REQUIRED)
include_directories(${TCLAP_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${TCLAP_LIBRARIES})

# LibAV
find_package(LIBAV REQUIRED)
include_directories(${LIBAV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${LIBAV_LIBRARIES})

# SDL2
pkg_search_module(SDL2 REQUIRED sdl2)
include_directories(${SDL2_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
