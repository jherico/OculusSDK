project(OculusSDK)

cmake_minimum_required(VERSION 2.8)

# Support our own custom modules
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/CMake/Modules
)

# First for the generic no-config case (e.g. with mingw)
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output )

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
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    endif()
endif()

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
elseif(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
else() 
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

# The primary SDK artifact, a static library for Oculus access 
add_subdirectory(LibOVR)
set_target_properties(ovr PROPERTIES FOLDER "Oculus")
