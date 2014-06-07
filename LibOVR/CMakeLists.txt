project (OculusVR)

include_directories(Include Src Src/Kernel Src/Util)
include_directories(../Bindings/C/Include)

file(GLOB_RECURSE SOURCE_FILES Src/*.cpp Src/*.h Include/*.h)
list(REMOVE_ITEM SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Src/OVR_Common_HMDDevice.cpp)

if(WIN32)
    # Extra source
    file(GLOB PLATFORM_SOURCE_FILES Platform/Win32/*.cpp Platform/Win32/*.h)
    list(APPEND PLATFORM_SOURCE_FILES Platform/OVR_ThreadsWinAPI.cpp)

    # Extra libs
    list(APPEND EXTRA_LIBS setupapi winmm)

elseif(APPLE)

    # Extra source
    file(GLOB PLATFORM_SOURCE_FILES Platform/OSX/*.cpp Platform/OSX/*.h)
    list(APPEND PLATFORM_SOURCE_FILES Platform/OVR_ThreadsPthread.cpp)

    # Extra libs
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    list(APPEND EXTRA_LIBS ${COCOA_LIBRARY} ${IOKIT_LIBRARY})

else()
    # Extra source
    file(GLOB PLATFORM_SOURCE_FILES Platform/Linux/*.cpp Platform/Linux/*.h)
    list(APPEND PLATFORM_SOURCE_FILES Platform/OVR_ThreadsPthread.cpp)

    # Extra libs
    # Used by the SDK to read the EDID of the monitors
    list(APPEND EXTRA_LIBS edid)

    # Used by the SDK to access the HID functions
    list(APPEND EXTRA_LIBS udev)

    find_package(Threads REQUIRED)
    list(APPEND EXTRA_LIBS ${CMAKE_THREAD_LIBS_INIT})

    # Check for XRandR (modern resolution switching and gamma control)
    find_package(X11 REQUIRED)
    if (NOT X11_Xrandr_FOUND)
        message(FATAL_ERROR "The RandR library and headers were not found")
    endif()
    list(APPEND EXTRA_LIBS ${X11_X11_LIB}) 
    list(APPEND EXTRA_LIBS ${X11_Xrandr_LIB})

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/EDID)

endif()

add_library(OculusVR STATIC ${SOURCE_FILES} ${PLATFORM_SOURCE_FILES})
set(OVR_LIBRARIES ${EXTRA_LIBS} CACHE STRING "Dependencies of OculusVR")
