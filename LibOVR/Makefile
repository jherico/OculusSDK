#############################################################################
#
# Filename    : Makefile
# Content     : Makefile for building linux version of: libovr
# Created     : 2013
# Authors     : Simon Hallam and Peter Giokaris
# Copyright   : Copyright 2013 OculusVR, Inc. All Rights Reserved
# Instruction : The g++ compiler and stdndard lib packages need to be 
#               installed on the system.  Navigate in a shell to the 
#               directory where this Makefile is located and enter:
#
#               make                builds the release version for the 
#                                   current architechture
#               make clean          delete intermediate release object files 
#                                   and the library file
#               make DEBUG=1        builds the debug version for the current
#                                   architechture
#               make clean DEBUG=1  deletes intermediate debug object files 
#                                   and the library file
#
# Output      : Relative to the directory this Makefile lives in, libraries
#               are built at the following locations depending upon the
#               architechture of the system you are running:
#
#               ./Lib/Linux/Debug/i386/libovr.a
#               ./Lib/Linux/Debug/x86_64/libovr.a
#               ./Lib/Linux/Release/i386/libovr.a
#               ./Lib/Linux/Release/x86_64/libovr.a
#
#############################################################################

####### Detect system architecture

SYSARCH       = i386
ifeq ($(shell uname -m),x86_64)
SYSARCH       = x86_64
endif

####### Compiler, tools and options

CXX           = g++
LINK          = ar rvs
DELETEFILE    = rm -f

####### Detect debug or release

DEBUG         = 0
ifeq ($(DEBUG), 1)
	CXXFLAGS      = -pipe -fPIC -DDEBUG -DOVR_BUILD_DEBUG -g
	RELEASETYPE   = Debug
else
	CXXFLAGS      = -pipe -fPIC -O2
	RELEASETYPE   = Release
endif

####### Paths

LIBOVRPATH    = .
3RDPARTYPATH  = ../3rdParty
INCPATH       = -I. -I.. -I$(LIBOVRPATH)/Include -I$(LIBOVRPATH)/Src
OBJPATH       = ./Obj/Linux/$(RELEASETYPE)/$(SYSARCH)
CXXBUILD      = $(CXX) -c $(CXXFLAGS) $(INCPATH) -o $(OBJPATH)/

####### Files

TARGET        = ./Lib/Linux/$(RELEASETYPE)/$(SYSARCH)/libovr.a

OBJECTS       = $(OBJPATH)/OVR_CAPI.o \
		$(OBJPATH)/CAPI_DistortionRenderer.o \
		$(OBJPATH)/CAPI_GL_DistortionRenderer.o \
		$(OBJPATH)/CAPI_GL_Util.o \
		$(OBJPATH)/CAPI_FrameTimeManager.o \
		$(OBJPATH)/CAPI_GlobalState.o \
		$(OBJPATH)/CAPI_HMDRenderState.o \
		$(OBJPATH)/CAPI_HMDState.o \
		$(OBJPATH)/OVR_DeviceHandle.o \
		$(OBJPATH)/OVR_DeviceImpl.o \
		$(OBJPATH)/OVR_JSON.o \
		$(OBJPATH)/OVR_LatencyTestImpl.o \
		$(OBJPATH)/OVR_Profile.o \
		$(OBJPATH)/OVR_Linux_SensorDevice.o\
		$(OBJPATH)/OVR_SensorCalibration.o\
		$(OBJPATH)/OVR_SensorFilter.o\
		$(OBJPATH)/OVR_SensorFusion.o\
		$(OBJPATH)/OVR_SensorImpl.o \
		$(OBJPATH)/OVR_Sensor2Impl.o \
		$(OBJPATH)/OVR_SensorImpl_Common.o \
		$(OBJPATH)/OVR_SensorTimeFilter.o \
		$(OBJPATH)/OVR_Stereo.o \
		$(OBJPATH)/OVR_ThreadCommandQueue.o \
		$(OBJPATH)/OVR_Alg.o \
		$(OBJPATH)/OVR_Allocator.o \
		$(OBJPATH)/OVR_Atomic.o \
		$(OBJPATH)/OVR_File.o \
		$(OBJPATH)/OVR_FileFILE.o \
		$(OBJPATH)/OVR_Log.o \
		$(OBJPATH)/OVR_Math.o \
                $(OBJPATH)/OVR_Recording.o \
		$(OBJPATH)/OVR_RefCount.o \
		$(OBJPATH)/OVR_Std.o \
		$(OBJPATH)/OVR_String.o \
		$(OBJPATH)/OVR_String_FormatUtil.o \
		$(OBJPATH)/OVR_String_PathUtil.o \
		$(OBJPATH)/OVR_SysFile.o \
		$(OBJPATH)/OVR_System.o \
		$(OBJPATH)/OVR_Timer.o \
		$(OBJPATH)/OVR_UTF8Util.o \
		$(OBJPATH)/Util_LatencyTest.o \
		$(OBJPATH)/Util_LatencyTest2.o \
		$(OBJPATH)/Util_Render_Stereo.o \
		$(OBJPATH)/OVR_ThreadsPthread.o \
		$(OBJPATH)/OVR_Linux_HIDDevice.o \
		$(OBJPATH)/OVR_Linux_SensorDevice.o \
		$(OBJPATH)/OVR_Linux_DeviceManager.o \
		$(OBJPATH)/OVR_Linux_HMDDevice.o \
                $(OBJPATH)/edid.o

####### Rules

all:    $(TARGET)

$(TARGET):  $(OBJECTS)
	$(LINK) $(TARGET) $(OBJECTS)

$(OBJPATH)/OVR_CAPI.o: $(LIBOVRPATH)/Src/OVR_CAPI.cpp
	$(CXXBUILD)OVR_CAPI.o $(LIBOVRPATH)/Src/OVR_CAPI.cpp

$(OBJPATH)/CAPI_DistortionRenderer.o: $(LIBOVRPATH)/Src/CAPI/CAPI_DistortionRenderer.cpp
	$(CXXBUILD)CAPI_DistortionRenderer.o $(LIBOVRPATH)/Src/CAPI/CAPI_DistortionRenderer.cpp

$(OBJPATH)/CAPI_GL_DistortionRenderer.o: $(LIBOVRPATH)/Src/CAPI/GL/CAPI_GL_DistortionRenderer.cpp
	$(CXXBUILD)CAPI_GL_DistortionRenderer.o $(LIBOVRPATH)/Src/CAPI/GL/CAPI_GL_DistortionRenderer.cpp

$(OBJPATH)/CAPI_GL_Util.o: $(LIBOVRPATH)/Src/CAPI/GL/CAPI_GL_Util.cpp
	$(CXXBUILD)CAPI_GL_Util.o $(LIBOVRPATH)/Src/CAPI/GL/CAPI_GL_Util.cpp

$(OBJPATH)/CAPI_FrameTimeManager.o: $(LIBOVRPATH)/Src/CAPI/CAPI_FrameTimeManager.cpp
	$(CXXBUILD)CAPI_FrameTimeManager.o $(LIBOVRPATH)/Src/CAPI/CAPI_FrameTimeManager.cpp

$(OBJPATH)/CAPI_GlobalState.o: $(LIBOVRPATH)/Src/CAPI/CAPI_GlobalState.cpp
	$(CXXBUILD)CAPI_GlobalState.o $(LIBOVRPATH)/Src/CAPI/CAPI_GlobalState.cpp

$(OBJPATH)/CAPI_HMDRenderState.o: $(LIBOVRPATH)/Src/CAPI/CAPI_HMDRenderState.cpp
	$(CXXBUILD)CAPI_HMDRenderState.o $(LIBOVRPATH)/Src/CAPI/CAPI_HMDRenderState.cpp

$(OBJPATH)/CAPI_HMDState.o: $(LIBOVRPATH)/Src/CAPI/CAPI_HMDState.cpp
	$(CXXBUILD)CAPI_HMDState.o $(LIBOVRPATH)/Src/CAPI/CAPI_HMDState.cpp

$(OBJPATH)/OVR_DeviceHandle.o: $(LIBOVRPATH)/Src/OVR_DeviceHandle.cpp 
	$(CXXBUILD)OVR_DeviceHandle.o $(LIBOVRPATH)/Src/OVR_DeviceHandle.cpp

$(OBJPATH)/OVR_DeviceImpl.o: $(LIBOVRPATH)/Src/OVR_DeviceImpl.cpp 
	$(CXXBUILD)OVR_DeviceImpl.o $(LIBOVRPATH)/Src/OVR_DeviceImpl.cpp

$(OBJPATH)/OVR_JSON.o: $(LIBOVRPATH)/Src/OVR_JSON.cpp 
	$(CXXBUILD)OVR_JSON.o $(LIBOVRPATH)/Src/OVR_JSON.cpp

$(OBJPATH)/OVR_LatencyTestImpl.o: $(LIBOVRPATH)/Src/OVR_LatencyTestImpl.cpp 
	$(CXXBUILD)OVR_LatencyTestImpl.o $(LIBOVRPATH)/Src/OVR_LatencyTestImpl.cpp

$(OBJPATH)/OVR_Profile.o: $(LIBOVRPATH)/Src/OVR_Profile.cpp 
	$(CXXBUILD)OVR_Profile.o $(LIBOVRPATH)/Src/OVR_Profile.cpp

$(OBJPATH)/OVR_SensorDevice.o: $(LIBOVRPATH)/Src/OVR_Linux_SensorDevice.cpp
	$(CXXBUILD)OVR_Linux_SensorDevice.o $(LIBOVRPATH)/Src/OVR_Linux_SensorDevice.cpp

$(OBJPATH)/OVR_SensorCalibration.o: $(LIBOVRPATH)/Src/OVR_SensorCalibration.cpp
	$(CXXBUILD)OVR_SensorCalibration.o $(LIBOVRPATH)/Src/OVR_SensorCalibration.cpp

$(OBJPATH)/OVR_SensorFilter.o: $(LIBOVRPATH)/Src/OVR_SensorFilter.cpp 
	$(CXXBUILD)OVR_SensorFilter.o $(LIBOVRPATH)/Src/OVR_SensorFilter.cpp

$(OBJPATH)/OVR_SensorFusion.o: $(LIBOVRPATH)/Src/OVR_SensorFusion.cpp 
	$(CXXBUILD)OVR_SensorFusion.o $(LIBOVRPATH)/Src/OVR_SensorFusion.cpp

$(OBJPATH)/OVR_SensorImpl.o: $(LIBOVRPATH)/Src/OVR_SensorImpl.cpp
	$(CXXBUILD)OVR_SensorImpl.o $(LIBOVRPATH)/Src/OVR_SensorImpl.cpp

$(OBJPATH)/OVR_Sensor2Impl.o: $(LIBOVRPATH)/Src/OVR_Sensor2Impl.cpp
	$(CXXBUILD)OVR_Sensor2Impl.o $(LIBOVRPATH)/Src/OVR_Sensor2Impl.cpp

$(OBJPATH)/OVR_SensorImpl_Common.o: $(LIBOVRPATH)/Src/OVR_SensorImpl_Common.cpp
	$(CXXBUILD)OVR_SensorImpl_Common.o $(LIBOVRPATH)/Src/OVR_SensorImpl_Common.cpp

$(OBJPATH)/OVR_SensorTimeFilter.o: $(LIBOVRPATH)/Src/OVR_SensorTimeFilter.cpp
	$(CXXBUILD)OVR_SensorTimeFilter.o $(LIBOVRPATH)/Src/OVR_SensorTimeFilter.cpp

$(OBJPATH)/OVR_Stereo.o: $(LIBOVRPATH)/Src/OVR_Stereo.cpp 
	$(CXXBUILD)OVR_Stereo.o $(LIBOVRPATH)/Src/OVR_Stereo.cpp

$(OBJPATH)/OVR_ThreadCommandQueue.o: $(LIBOVRPATH)/Src/OVR_ThreadCommandQueue.cpp 
	$(CXXBUILD)OVR_ThreadCommandQueue.o $(LIBOVRPATH)/Src/OVR_ThreadCommandQueue.cpp

$(OBJPATH)/OVR_Alg.o: $(LIBOVRPATH)/Src/Kernel/OVR_Alg.cpp 
	$(CXXBUILD)OVR_Alg.o $(LIBOVRPATH)/Src/Kernel/OVR_Alg.cpp

$(OBJPATH)/OVR_Allocator.o: $(LIBOVRPATH)/Src/Kernel/OVR_Allocator.cpp 
	$(CXXBUILD)OVR_Allocator.o $(LIBOVRPATH)/Src/Kernel/OVR_Allocator.cpp

$(OBJPATH)/OVR_Atomic.o: $(LIBOVRPATH)/Src/Kernel/OVR_Atomic.cpp 
	$(CXXBUILD)OVR_Atomic.o $(LIBOVRPATH)/Src/Kernel/OVR_Atomic.cpp

$(OBJPATH)/OVR_File.o: $(LIBOVRPATH)/Src/Kernel/OVR_File.cpp 
	$(CXXBUILD)OVR_File.o $(LIBOVRPATH)/Src/Kernel/OVR_File.cpp

$(OBJPATH)/OVR_FileFILE.o: $(LIBOVRPATH)/Src/Kernel/OVR_FileFILE.cpp 
	$(CXXBUILD)OVR_FileFILE.o $(LIBOVRPATH)/Src/Kernel/OVR_FileFILE.cpp

$(OBJPATH)/OVR_Log.o: $(LIBOVRPATH)/Src/Kernel/OVR_Log.cpp 
	$(CXXBUILD)OVR_Log.o $(LIBOVRPATH)/Src/Kernel/OVR_Log.cpp

$(OBJPATH)/OVR_Math.o: $(LIBOVRPATH)/Src/Kernel/OVR_Math.cpp 
	$(CXXBUILD)OVR_Math.o $(LIBOVRPATH)/Src/Kernel/OVR_Math.cpp

$(OBJPATH)/OVR_Recording.o: $(LIBOVRPATH)/Src/OVR_Recording.cpp 
	$(CXXBUILD)OVR_Recording.o $(LIBOVRPATH)/Src/OVR_Recording.cpp

$(OBJPATH)/OVR_RefCount.o: $(LIBOVRPATH)/Src/Kernel/OVR_RefCount.cpp 
	$(CXXBUILD)OVR_RefCount.o $(LIBOVRPATH)/Src/Kernel/OVR_RefCount.cpp

$(OBJPATH)/OVR_Std.o: $(LIBOVRPATH)/Src/Kernel/OVR_Std.cpp 
	$(CXXBUILD)OVR_Std.o $(LIBOVRPATH)/Src/Kernel/OVR_Std.cpp

$(OBJPATH)/OVR_String.o: $(LIBOVRPATH)/Src/Kernel/OVR_String.cpp 
	$(CXXBUILD)OVR_String.o $(LIBOVRPATH)/Src/Kernel/OVR_String.cpp

$(OBJPATH)/OVR_String_FormatUtil.o: $(LIBOVRPATH)/Src/Kernel/OVR_String_FormatUtil.cpp 
	$(CXXBUILD)OVR_String_FormatUtil.o $(LIBOVRPATH)/Src/Kernel/OVR_String_FormatUtil.cpp

$(OBJPATH)/OVR_String_PathUtil.o: $(LIBOVRPATH)/Src/Kernel/OVR_String_PathUtil.cpp
	$(CXXBUILD)OVR_String_PathUtil.o $(LIBOVRPATH)/Src/Kernel/OVR_String_PathUtil.cpp

$(OBJPATH)/OVR_SysFile.o: $(LIBOVRPATH)/Src/Kernel/OVR_SysFile.cpp 
	$(CXXBUILD)OVR_SysFile.o $(LIBOVRPATH)/Src/Kernel/OVR_SysFile.cpp

$(OBJPATH)/OVR_System.o: $(LIBOVRPATH)/Src/Kernel/OVR_System.cpp 
	$(CXXBUILD)OVR_System.o $(LIBOVRPATH)/Src/Kernel/OVR_System.cpp

$(OBJPATH)/OVR_Timer.o: $(LIBOVRPATH)/Src/Kernel/OVR_Timer.cpp 
	$(CXXBUILD)OVR_Timer.o $(LIBOVRPATH)/Src/Kernel/OVR_Timer.cpp

$(OBJPATH)/OVR_UTF8Util.o: $(LIBOVRPATH)/Src/Kernel/OVR_UTF8Util.cpp 
	$(CXXBUILD)OVR_UTF8Util.o $(LIBOVRPATH)/Src/Kernel/OVR_UTF8Util.cpp

$(OBJPATH)/Util_LatencyTest.o: $(LIBOVRPATH)/Src/Util/Util_LatencyTest.cpp 
	$(CXXBUILD)Util_LatencyTest.o $(LIBOVRPATH)/Src/Util/Util_LatencyTest.cpp

$(OBJPATH)/Util_LatencyTest2.o: $(LIBOVRPATH)/Src/Util/Util_LatencyTest2.cpp
	$(CXXBUILD)Util_LatencyTest2.o $(LIBOVRPATH)/Src/Util/Util_LatencyTest2.cpp

$(OBJPATH)/Util_Render_Stereo.o: $(LIBOVRPATH)/Src/Util/Util_Render_Stereo.cpp
	$(CXXBUILD)Util_Render_Stereo.o $(LIBOVRPATH)/Src/Util/Util_Render_Stereo.cpp

$(OBJPATH)/OVR_ThreadsPthread.o: $(LIBOVRPATH)/Src/Kernel/OVR_ThreadsPthread.cpp 
	$(CXXBUILD)OVR_ThreadsPthread.o $(LIBOVRPATH)/Src/Kernel/OVR_ThreadsPthread.cpp

$(OBJPATH)/OVR_Linux_HIDDevice.o: $(LIBOVRPATH)/Src/OVR_Linux_HIDDevice.cpp 
	$(CXXBUILD)OVR_Linux_HIDDevice.o $(LIBOVRPATH)/Src/OVR_Linux_HIDDevice.cpp

$(OBJPATH)/OVR_Linux_SensorDevice.o: $(LIBOVRPATH)/Src/OVR_Linux_SensorDevice.cpp 
	$(CXXBUILD)OVR_Linux_SensorDevice.o $(LIBOVRPATH)/Src/OVR_Linux_SensorDevice.cpp

$(OBJPATH)/OVR_Linux_DeviceManager.o: $(LIBOVRPATH)/Src/OVR_Linux_DeviceManager.cpp 
	$(CXXBUILD)OVR_Linux_DeviceManager.o $(LIBOVRPATH)/Src/OVR_Linux_DeviceManager.cpp

$(OBJPATH)/OVR_Linux_HMDDevice.o: $(LIBOVRPATH)/Src/OVR_Linux_HMDDevice.cpp 
	$(CXXBUILD)OVR_Linux_HMDDevice.o $(LIBOVRPATH)/Src/OVR_Linux_HMDDevice.cpp

$(OBJPATH)/tinyxml2.o: $(3RDPARTYPATH)/TinyXml/tinyxml2.cpp 
	$(CXXBUILD)tinyxml2.o $(3RDPARTYPATH)/TinyXml/tinyxml2.cpp

$(OBJPATH)/edid.o: $(3RDPARTYPATH)/EDID/edid.cpp 
	$(CXXBUILD)edid.o $(3RDPARTYPATH)/EDID/edid.cpp

clean:
	-$(DELETEFILE) $(OBJECTS)
	-$(DELETEFILE) $(TARGET)

