cmake_minimum_required(VERSION 3.3)
project(Lab1)
add_definitions(-DWIN32)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
#server
set(SERVER_FILES server.cpp david.cpp)

add_executable(server ${SERVER_FILES} david.cpp)
target_link_libraries(server wsock32 ws2_32)