project (OVR_C)

include_directories(Include Src)

# Make sure to set up the OVR include paths
include_directories(../../3rdParty/glext)
include_directories(${LibOVR_SOURCE_DIR}/Include)
include_directories(${LibOVR_SOURCE_DIR}/Src)
include_directories(${LibOVR_SOURCE_DIR}/Src/Kernel)
include_directories(${LibOVR_SOURCE_DIR}/Src/Util)

file(GLOB_RECURSE SOURCE_FILES Src/*.cpp Src/*.h Include/*.h)

if(WIN32)
    list(APPEND OculusVR_EXTRA_LIBS setupapi winmm)

elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    list(APPEND OculusVR_EXTRA_LIBS ${COCOA_LIBRARY} ${IOKIT_LIBRARY})
else()
    # Used by the SDK to read the EDID of the monitors
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/EDID 
        ${CMAKE_CURRENT_BINARY_DIR}/../3rdParty/EDID
    )
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/EDID)

    find_package(X11 REQUIRED)
    find_package (Threads REQUIRED)

    # Check for XRandR (modern resolution switching and gamma control)
    if (NOT X11_Xrandr_FOUND)
        message(FATAL_ERROR "The RandR library and headers were not found")
    endif()

    # Set up library and include paths
    list(APPEND OculusVR_EXTRA_LIBS 
        ${X11_X11_LIB} 
        ${X11_Xrandr_LIB}
        ${CMAKE_THREAD_LIBS_INIT}
        edid udev
    )
endif()

add_library(OVR_C SHARED ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(OVR_C OculusVR ${OculusVR_EXTRA_LIBS})
