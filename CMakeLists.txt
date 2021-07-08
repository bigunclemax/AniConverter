cmake_minimum_required(VERSION 3.15)
project(AniConverter)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(h264bitstream EXCLUDE_FROM_ALL)

add_executable(ani2h264raw ani2h264raw.cpp)

add_executable(h264raw2ani h264raw2ani.cpp)
target_link_libraries(h264raw2ani PRIVATE h264bitstream)