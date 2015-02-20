project (ovr)

include_directories(Include Src Src/Kernel Src/Util)
include_directories(../3rdParty/glext)

file(GLOB_RECURSE SOURCE_FILES Src/*.cpp Src/*.h Include/*.h)

file(GLOB_RECURSE WIN32_FILES Src/OVR_Win32_*.cpp Src/OVR_Win32_*.h Src/Kernel/OVR_ThreadsWinAPI.cpp *D3D*.cpp)
file(GLOB_RECURSE D3DX_FILES Src/CAPI_D3D1X*.cpp Src/OVR_D3D1X*.h)
file(GLOB_RECURSE OSX_FILES Src/OVR_OSX_*.cpp Src/OVR_OSX_*.h Src/*.mm Src/*OSX.c)
file(GLOB_RECURSE LINUX_FILES Src/OVR_Linux_*.cpp Src/OVR_Linux_*.h)
file(GLOB_RECURSE UNIX_FILES Src/OVR_Unix_*.cpp Src/OVR_Unix_*.h Src/Kernel/OVR_ThreadsPthread.cpp)

foreach(f ${OSX_FILES}) 
    list(REMOVE_ITEM SOURCE_FILES ${f})
endforeach(f) 

foreach(f ${UNIX_FILES}) 
    list(REMOVE_ITEM SOURCE_FILES ${f})
endforeach(f) 

foreach(f ${LINUX_FILES}) 
    list(REMOVE_ITEM SOURCE_FILES ${f})
endforeach(f) 

foreach(f ${WIN32_FILES}) 
    list(REMOVE_ITEM SOURCE_FILES ${f})
endforeach(f) 

if(WIN32)

    # Extra source
    list(APPEND SOURCE_FILES ${WIN32_FILES})
    foreach(f ${D3DX_FILES}) 
      list(REMOVE_ITEM SOURCE_FILES ${f})
    endforeach(f) 


    # Extra libs
    list(APPEND EXTRA_LIBS setupapi winmm ws2_32)

elseif(APPLE)

    # Extra source
    list(APPEND SOURCE_FILES ${OSX_FILES})
    list(APPEND SOURCE_FILES ${UNIX_FILES})

    # Some special files that apparently don’t belong in the SDK ?!
    file(GLOB_RECURSE OSX_PRIVATE_FILES Src/Displays/OVR_OSX_Focus*)
    foreach(f ${OSX_PRIVATE_FILES}) 
        list(REMOVE_ITEM SOURCE_FILES ${f})
    endforeach(f) 

    # Extra libs
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    list(APPEND EXTRA_LIBS ${COCOA_LIBRARY} ${IOKIT_LIBRARY})

    find_package(OpenGL REQUIRED)
    list(APPEND EXTRA_LIBS ${OPENGL_LIBRARIES})

else()

    # Extra source
    list(APPEND SOURCE_FILES ${LINUX_FILES})
    list(APPEND SOURCE_FILES ${UNIX_FILES})

    # Extra libs
	list(APPEND SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/EDID/edid.cpp)    

    # Used by the SDK to read the EDID of the monitors
    list(APPEND EXTRA_LIBS rt)

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

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DOVR_BUILD_DEBUG")
add_library(ovr ${SOURCE_FILES} ${PLATFORM_SOURCE_FILES})
target_link_libraries(ovr ${EXTRA_LIBS})
set(OVR_LIBRARIES ${EXTRA_LIBS} CACHE STRING "Dependencies of OculusVR")

install(TARGETS ovr DESTINATION lib)
install(
    FILES 
        Include/OVR_Version.h
        Src/Kernel/OVR_Types.h
        Src/OVR_CAPI.h
        Src/OVR_CAPI_GL.h
    DESTINATION include)
