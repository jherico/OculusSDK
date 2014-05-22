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
    add_definitions(-std=gnu++0x)
    if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fPIC")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fPIC")
    endif()
endif()

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
elseif(APPLE)
else() 
    add_subdirectory(3rdParty/EDID)
    set_target_properties(edid PROPERTIES FOLDER "3rdParty")
endif()

# The primary SDK artifact, a static library for Oculus access 
add_subdirectory(LibOVR)
set_target_properties(OculusVR PROPERTIES FOLDER "Oculus")

# Cross platform GL access.  Used by the C API and by
# the sample applications
add_definitions(-DGLEW_STATIC)
add_subdirectory(3rdParty/glew)
set_target_properties(glew PROPERTIES FOLDER "3rdParty")

add_subdirectory(Bindings/C)
set_target_properties(OVR_C PROPERTIES FOLDER "Oculus")

###############################################################################
#
# Sample code support
#

option(OCULUS_BUILD_SAMPLES "Build Oculus demos" TRUE)

if(WIN32)
if(OCULUS_BUILD_SAMPLES)
    # Make sure to set up the OVR include paths
    include_directories(3rdParty/glew/include)
    include_directories(${OculusVR_SOURCE_DIR}/Include)
    include_directories(${OculusVR_SOURCE_DIR}/Src)
    include_directories(${OculusVR_SOURCE_DIR}/Src/Kernel)
    include_directories(${OculusVR_SOURCE_DIR}/Src/Util)
    include_directories(Bindings/C/Include)
    # Cross platform window creation
 #   add_subdirectory(3rdParty/glfw)
 #   include_directories(${CMAKE_SOURCE_DIR}/3rdParty/glfw/include)

    # Cross platform access to shader functionality
#    include_directories(${CMAKE_SOURCE_DIR}/3rdParty/glext)

    # Used by the sample code to read the tuscany resources
    add_subdirectory(3rdParty/TinyXml)
    set_target_properties(TinyXML2 PROPERTIES FOLDER "3rdParty")
    include_directories(3rdParty/TinyXml)

    add_subdirectory (Samples/CommonSrc )
    set_target_properties(CommonSrc PROPERTIES FOLDER "Samples")

    add_subdirectory (Samples/OculusWorldDemo )
    set_target_properties(OculusWorldDemo PROPERTIES FOLDER "Samples")
endif()
endif()
