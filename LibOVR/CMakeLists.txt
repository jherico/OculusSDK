project (LibOVR)

include_directories(Include Src Src/Kernel Src/Util)

set(SOURCE_FILES
    Src/Kernel/OVR_Alg.cpp
    Src/Kernel/OVR_Allocator.cpp
    Src/Kernel/OVR_Atomic.cpp
    Src/Kernel/OVR_File.cpp
    Src/Kernel/OVR_FileFILE.cpp
    Src/Kernel/OVR_Log.cpp
    Src/Kernel/OVR_Math.cpp
    Src/Kernel/OVR_RefCount.cpp
    Src/Kernel/OVR_Std.cpp
    Src/Kernel/OVR_String.cpp
    Src/Kernel/OVR_String_FormatUtil.cpp
    Src/Kernel/OVR_String_PathUtil.cpp
    Src/Kernel/OVR_SysFile.cpp
    Src/Kernel/OVR_System.cpp
    Src/Kernel/OVR_Timer.cpp
    Src/Kernel/OVR_UTF8Util.cpp
    Src/OVR_DeviceHandle.cpp
    Src/OVR_DeviceImpl.cpp
    Src/OVR_JSON.cpp
    Src/OVR_LatencyTestImpl.cpp
    Src/OVR_Profile.cpp
    Src/OVR_SensorFilter.cpp
    Src/OVR_SensorFusion.cpp
    Src/OVR_SensorImpl.cpp
    Src/OVR_ThreadCommandQueue.cpp
    Src/Util/Util_LatencyTest.cpp
    Src/Util/Util_Render_Stereo.cpp
)

set(HEADER_FILES
    Include/OVR.h
    Include/OVRVersion.h 
    Src/Kernel/OVR_Alg.h
    Src/Kernel/OVR_Allocator.h
    Src/Kernel/OVR_Array.h
    Src/Kernel/OVR_Atomic.h
    Src/Kernel/OVR_Color.h
    Src/Kernel/OVR_ContainerAllocator.h
    Src/Kernel/OVR_File.h
    Src/Kernel/OVR_Hash.h
    Src/Kernel/OVR_KeyCodes.h
    Src/Kernel/OVR_List.h
    Src/Kernel/OVR_Log.h
    Src/Kernel/OVR_Math.h
    Src/Kernel/OVR_RefCount.h
    Src/Kernel/OVR_Std.h
    Src/Kernel/OVR_String.h
    Src/Kernel/OVR_SysFile.h
    Src/Kernel/OVR_System.h
    Src/Kernel/OVR_Timer.h
    Src/Kernel/OVR_Threads.h
    Src/Kernel/OVR_Types.h
    Src/Kernel/OVR_UTF8Util.h
    Src/OVR_Device.h
    Src/OVR_DeviceConstants.h
    Src/OVR_DeviceHandle.h
    Src/OVR_DeviceImpl.h
    Src/OVR_DeviceMessages.h
    Src/OVR_HIDDevice.h
    Src/OVR_HIDDeviceBase.h
    Src/OVR_HIDDeviceImpl.h
    Src/OVR_JSON.h
    Src/OVR_LatencyTestImpl.h
    Src/OVR_Profile.h
    Src/OVR_SensorFilter.h
    Src/OVR_SensorFusion.h
    Src/OVR_SensorImpl.h
    Src/OVR_ThreadCommandQueue.h
    Src/Util/Util_LatencyTest.h
    Src/Util/Util_Render_Stereo.h
)

if(WIN32)
    list(APPEND SOURCE_FILES 
        Src/Kernel/OVR_ThreadsWinAPI.cpp
        Src/OVR_Win32_DeviceManager.cpp
        Src/OVR_Win32_DeviceStatus.cpp
        Src/OVR_Win32_HIDDevice.cpp
        Src/OVR_Win32_HMDDevice.cpp
        Src/OVR_Win32_SensorDevice.cpp
    )
    list(APPEND HEADER_FILES
        Src/OVR_Win32_DeviceManager.h
        Src/OVR_Win32_DeviceStatus.h
        Src/OVR_Win32_HIDDevice.h
        Src/OVR_Win32_HMDDevice.h
        Src/OVR_Win32_SensorDevice.h
    )
elseif(APPLE)
    list(APPEND SOURCE_FILES 
        Src/Kernel/OVR_ThreadsPthread.cpp
        Src/OVR_OSX_DeviceManager.cpp
        Src/OVR_OSX_HIDDevice.cpp
        Src/OVR_OSX_HMDDevice.cpp
        Src/OVR_OSX_SensorDevice.cpp
    )
    list(APPEND HEADER_FILES
        Src/OVR_OSX_DeviceManager.h
        Src/OVR_OSX_HIDDevice.h
        Src/OVR_OSX_HMDDevice.h
    )
else()
    list(APPEND SOURCE_FILES 
        Src/Kernel/OVR_ThreadsPthread.cpp
        Src/OVR_Linux_DeviceManager.cpp
        Src/OVR_Linux_HIDDevice.cpp
        Src/OVR_Linux_HMDDevice.cpp
        Src/OVR_Linux_SensorDevice.cpp
    )
    list(APPEND HEADER_FILES
        Src/OVR_Linux_DeviceManager.h
        Src/OVR_Linux_HIDDevice.h
        Src/OVR_Linux_HMDDevice.h
    )
    list(APPEND EXTRA_LIBS
        edid
    )
    include_directories(${LibEDID_SOURCE_DIR})
endif()

if(WIN32)

    SET(OculusVR_EXTRA_LIBS setupapi winmm)

elseif(APPLE)

    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    set(OculusVR_EXTRA_LIBS ${COCOA_LIBRARY} ${IOKIT_LIBRARY})

else()

    # FIXME switch to pkg-config for this?
    SET(OculusVR_EXTRA_LIBS edid pthread udev X11 Xrandr)

endif()

add_library(OculusVR STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(OculusVR ${OculusVR_EXTRA_LIBS})
