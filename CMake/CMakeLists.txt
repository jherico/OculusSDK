project(CORE_CMake)

cmake_minimum_required(VERSION 2.8)

# Support our own custom modules
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/CMake/Modules
)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_VERBOSE_MAKEFILE ON)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-std=gnu++0x)
    if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC"  )
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC"  )    
    endif()
endif()

if(WIN32)
	add_definitions(-DUNICODE -D_UNICODE)
endif()

# Used by the SDK to read the EDID of the monitors
add_subdirectory(../3rdParty/EDID "${CMAKE_CURRENT_BINARY_DIR}/3rdParty/EDID")

# The primary SDK artifact, a static library for Oculus access 
add_subdirectory(../LibOVR "${CMAKE_CURRENT_BINARY_DIR}/LibOVR")

###############################################################################
#
# Sample code support
#

# Make sure to set up the OVR include paths
include_directories(${CORE_OculusVR_SOURCE_DIR}/Include)
include_directories(${CORE_OculusVR_SOURCE_DIR}/Src)

# Used by the sample code to read the tuscany resources
add_subdirectory(../3rdParty/TinyXml "${CMAKE_CURRENT_BINARY_DIR}/3rdParty/TinyXml" )
include_directories(${LIB_TinyXML2_SOURCE_DIR})

add_definitions( -DGL_GLEXT_PROTOTYPES )

add_subdirectory (../Samples/CommonSrc "${CMAKE_CURRENT_BINARY_DIR}/Samples/CommonSrc")
add_subdirectory (../Samples/OculusWorldDemo "${CMAKE_CURRENT_BINARY_DIR}/Samples/OculusWorldDemo")
#add_subdirectory (../Samples/OculusRoomTiny)


