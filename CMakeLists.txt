project(OculusSDK)

cmake_minimum_required(VERSION 2.8)

# Support our own custom modules
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/CMake/Modules
)

# Comment out these two lines to disable the debug build
set(CMAKE_BUILD_TYPE "Debug")
add_definitions(-DOVR_BUILD_DEBUG)

#set(CMAKE_VERBOSE_MAKEFILE ON)

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

include_directories(${CMAKE_SOURCE_DIR}/3rdParty/glext)
include_directories(${CMAKE_SOURCE_DIR}/LibOVR/Src/CAPI/D3D1X)

# The primary SDK artifact, a static library for Oculus access 
add_subdirectory(LibOVR)
add_subdirectory(Bindings/C)

###############################################################################
#
# Sample code support
#

option(OCULUS_BUILD_SAMPLES "Build Oculus demos" TRUE)

if(OCULUS_BUILD_SAMPLES)
    # Make sure to set up the OVR include paths
    include_directories(${LibOVR_SOURCE_DIR}/Include)
    include_directories(${LibOVR_SOURCE_DIR}/Src)
    include_directories(${LibOVR_SOURCE_DIR}/Src/Kernel)
    include_directories(${LibOVR_SOURCE_DIR}/Src/Util)
    include_directories(Bindings/C/Include)
    # Cross platform window creation
 #   add_subdirectory(3rdParty/glfw)
 #   include_directories(${CMAKE_SOURCE_DIR}/3rdParty/glfw/include)

    # Cross platform access to shader functionality
 #   add_definitions( -DGLEW_STATIC )
 #   add_subdirectory(3rdParty/glew)
#    include_directories(${CMAKE_SOURCE_DIR}/3rdParty/glew/include)
    include_directories(${CMAKE_SOURCE_DIR}/3rdParty/glext)

    # Used by the sample code to read the tuscany resources
    add_subdirectory(3rdParty/TinyXml)
    include_directories(${CMAKE_SOURCE_DIR}/3rdParty/TinyXml)

    add_definitions( -DGL_GLEXT_PROTOTYPES )
    #add_subdirectory (Samples/CommonSrc )
    #add_subdirectory (Samples/OculusWorldDemo )
    #add_subdirectory (Samples/OculusRoomTiny )
    #add_subdirectory (Samples/SensorBox )
    #add_subdirectory (Samples/TestSensor )
endif()

