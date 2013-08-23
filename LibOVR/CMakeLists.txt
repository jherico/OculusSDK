project (OculusVR_LIB)

include_directories(Include)
include_directories(C)

set(SOURCE_FILES
    C/OVR_impl.cpp
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
endif()

add_library(OculusVR SHARED ${SOURCE_FILES})
add_library(OculusVR_static STATIC ${SOURCE_FILES})

install(TARGETS OculusVR
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
