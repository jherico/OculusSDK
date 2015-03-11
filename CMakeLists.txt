project(OculusSDK)

cmake_minimum_required(VERSION 2.8)

# Support our own custom modules
include(cmake/defaults.cmake)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake )

# Official path is like so: ${CMAKE_BINARY_DIR}/Lib/(Platform)/(Buildconfig)/(Arch)/libovr.a

# First for the generic no-config case (e.g. with mingw)
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output )

include(CheckCXXCompilerFlag)
macro(check_and_add_flag var flag)
    CHECK_CXX_COMPILER_FLAG(${flag} FLAG_${var})
    if(FLAG_${var})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endif()
endmacro()

option(ENABLE_LTO "Build with Link-Time Optimization" FALSE)
    if(ENABLE_LTO)
    check_and_add_flag(LTO -flto)
    if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    set(CMAKE_AR gcc-ar)
    set(CMAKE_RANLIB gcc-ranlib)
    endif()
endif()

# Second, for multi-config builds (e.g. msvc)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/output )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/output )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/output )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DOVR_BUILD_DEBUG")
set(CMAKE_DEBUG_POSTFIX "d")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

if(CMAKE_COMPILER_IS_GNUCXX)
    if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -fPIC")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -fPIC")
    endif()
	# Use official default CFLAGS
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -msse2 -fno-strict-aliasing")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -msse2 -fno-strict-aliasing")
endif()

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
elseif(APPLE)
else() 
endif()

# The primary SDK artifact, a static library for Oculus access 
add_subdirectory(LibOVR)
set_target_properties(ovr PROPERTIES FOLDER "Oculus")

###############################################################################
#
# Sample code support
#

option(OCULUS_BUILD_SAMPLES "Build Oculus demos" FALSE)
if(OCULUS_BUILD_SAMPLES)
    # Used by the sample code to read the tuscany resources
    add_subdirectory(3rdParty/TinyXml)
    set_target_properties(TinyXML2 PROPERTIES FOLDER "3rdParty")
    include_directories(3rdParty/TinyXml)

    # Make sure to set up the OVR include paths
    include_directories(${ovr_SOURCE_DIR}/Include)
    include_directories(${ovr_SOURCE_DIR}/Src)
    include_directories(${ovr_SOURCE_DIR}/Src/Kernel)
    include_directories(${ovr_SOURCE_DIR}/Src/Util)

    add_subdirectory (Samples/CommonSrc )
    set_target_properties(CommonSrc PROPERTIES FOLDER "Samples")

    add_subdirectory (Samples/OculusWorldDemo )
    set_target_properties(OculusWorldDemo PROPERTIES FOLDER "Samples")

    if (WIN32)
        add_subdirectory (Samples/OculusRoomTiny )
        set_target_properties(OculusRoomTiny PROPERTIES FOLDER "Samples")
    endif()
    
endif()
