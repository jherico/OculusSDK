project (LibOVR)

include_directories(Include Src Src/Kernel Src/Util)
include_directories(../3rdParty/glext)

file(GLOB_RECURSE SOURCE_FILES Src/*.cpp Src/*.h)
list(REMOVE_ITEM SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Src/OVR_Common_HMDDevice.cpp)

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


add_library(OculusVR STATIC ${SOURCE_FILES} ${HEADER_FILES})
set(OVR_LIBRARIES ${OculusVR_EXTRA_LIBS} CACHE STRING "Dependencies of OculusVR")
target_link_libraries(OculusVR ${OculusVR_EXTRA_LIBS})
