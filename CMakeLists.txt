cmake_minimum_required(VERSION 3.30)
project(hpc_project
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG -Wall -Wpedantic")

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

if(NOT WIN32)
  string(ASCII 27 Esc)
  set(ColorReset "${Esc}[m")
  set(Cyan        "${Esc}[36m")
endif()

message(STATUS "${Cyan}CMAKE_BUILD_TYPE${ColorReset} = ${CMAKE_BUILD_TYPE}")
message(STATUS "${Cyan}CMAKE_CXX_STANDARD${ColorReset} = ${CMAKE_CXX_STANDARD}")
message(STATUS "${Cyan}CMAKE_CXX_COMPILER${ColorReset} = ${CMAKE_CXX_COMPILER}")
message(STATUS "${Cyan}CMAKE_CXX_COMPILER_ID${ColorReset} = ${CMAKE_CXX_COMPILER_ID}")

add_subdirectory(src)
