cmake_minimum_required(VERSION 3.10)
project(ShortVideoRank)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    /usr/include/mysql
)

file(GLOB_RECURSE SOURCES
    "src/*.cpp"
)

add_executable(
    ShortVideoRank
    main.cpp
    ${SOURCES}
)

target_link_libraries(
    ShortVideoRank
    muduo_net
    muduo_base
    pthread
    mysqlclient
)
