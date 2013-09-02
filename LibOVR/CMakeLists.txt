project (CORE_OculusVR)

include_directories(Include)

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
    Src/Util/Util_MagCalibration.cpp
    Src/Util/Util_Render_Stereo.cpp
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
elseif(APPLE)
    list(APPEND SOURCE_FILES 
        Src/Kernel/OVR_ThreadsPthread.cpp
        Src/OVR_OSX_DeviceManager.cpp
        Src/OVR_OSX_HIDDevice.cpp
        Src/OVR_OSX_HMDDevice.cpp
        Src/OVR_OSX_SensorDevice.cpp
    )
else()
    list(APPEND SOURCE_FILES 
        Src/Kernel/OVR_ThreadsPthread.cpp
        Src/OVR_Linux_DeviceManager.cpp
        Src/OVR_Linux_HIDDevice.cpp
        Src/OVR_Linux_HMDDevice.cpp
        Src/OVR_Linux_SensorDevice.cpp
    )
    list(APPEND EXTRA_LIBS
		# Should this be applied to all platforms?
        EDID
    )
    include_directories(${EDID_LIB_SOURCE_DIR})
endif()

if(WIN32)
    SET(OculusVR_EXTRA_LIBS setupapi winmm)
elseif(APPLE)
    # FIXME ???
else()
    # FIXME switch to pkg-config for this?
    SET(OculusVR_EXTRA_LIBS pthread udev X11 Xrandr Xinerama)
endif()

add_library(OculusVR STATIC ${SOURCE_FILES})
target_link_libraries(OculusVR EDID ${OculusVR_EXTRA_LIBS})
