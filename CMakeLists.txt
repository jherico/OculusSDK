project(OculusVR_SDK)

cmake_minimum_required(VERSION 2.8)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-std=gnu++0x)
endif()

add_subdirectory(LibOVR)
add_subdirectory(3rdParty)
add_subdirectory(Samples)

set(CMAKE_VERBOSE_MAKEFILE ON)

