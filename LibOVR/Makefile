#############################################################################
#
# Filename    : Makefile
# Content     : Makefile for building linux version of: libovr
# Created     : 2014
# Authors     : Simon Hallam, Peter Giokaris, Chris Taylor
# Copyright   : Copyright 2014 OculusVR, Inc. All Rights Reserved
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
LINK          = 
DELETEFILE    = rm -f

####### Detect debug or release

COMMONFLAGS   = -Wall -Wextra -pipe -fPIC -msse2

DEBUG         ?= 0
CXXFLAGS 			?=
ifeq ($(DEBUG), 1)
	CXXFLAGS      += $(COMMONFLAGS) -DDEBUG -DOVR_BUILD_DEBUG -g
	RELEASETYPE   ?= Debug
else
	CXXFLAGS      += $(COMMONFLAGS) -O2
	RELEASETYPE   ?= Release
endif

SINGLE_PROCESS    ?= 0
ifeq ($(SINGLE_PROCESS), 1)
	CXXFLAGS      += -DOVR_SINGLE_PROCESS
endif

####### Paths

LIBOVRPATH    = .
3RDPARTYPATH  = ../3rdParty
INCPATH       = -I. -I.. -I$(LIBOVRPATH)/Include -I$(LIBOVRPATH)/Src
OBJPATH       = ./Obj/Linux/$(RELEASETYPE)/$(SYSARCH)

####### Files

STATIC_TARGET = ./Lib/Linux/$(RELEASETYPE)/$(SYSARCH)/libovr.a
SHARED_TARGET	= ./Lib/Linux/$(RELEASETYPE)/$(SYSARCH)/libovr.so
SHARED_MAJOR = 0
SHARED_MINOR = 1
SHARED_RELEASE = 1

####### Rules

all:    $(STATIC_TARGET) $(SHARED_TARGET)

OTHER_SOURCE =	$(3RDPARTYPATH)/EDID/edid.cpp \
				$(3RDPARTYPATH)/TinyXml/tinyxml2.cpp

OTHER_OBJECTS = $(OTHER_SOURCE:.cpp=.o)

LIBOVR_SOURCE = 	$(LIBOVRPATH)/Src/Net/OVR_BitStream.cpp \
				$(LIBOVRPATH)/Src/Net/OVR_Unix_Socket.cpp \
				$(LIBOVRPATH)/Src/Net/OVR_NetworkPlugin.cpp \
				$(LIBOVRPATH)/Src/Net/OVR_PacketizedTCPSocket.cpp \
				$(LIBOVRPATH)/Src/Net/OVR_RPC1.cpp \
				$(LIBOVRPATH)/Src/Net/OVR_Session.cpp \
				$(LIBOVRPATH)/Src/Net/OVR_Socket.cpp \
				$(LIBOVRPATH)/Src/Service/Service_NetClient.cpp \
				$(LIBOVRPATH)/Src/Service/Service_NetSessionCommon.cpp \
				$(LIBOVRPATH)/Src/Tracking/Tracking_SensorStateReader.cpp \
				$(LIBOVRPATH)/Src/Displays/OVR_Display.cpp \
				$(LIBOVRPATH)/Src/Displays/OVR_Linux_Display.cpp \
				$(LIBOVRPATH)/Src/Displays/OVR_Linux_SDKWindow.cpp \
				$(LIBOVRPATH)/Src/CAPI/CAPI_DistortionRenderer.cpp \
				$(LIBOVRPATH)/Src/CAPI/CAPI_HSWDisplay.cpp \
				$(LIBOVRPATH)/Src/CAPI/CAPI_FrameTimeManager.cpp \
				$(LIBOVRPATH)/Src/CAPI/CAPI_HMDRenderState.cpp \
				$(LIBOVRPATH)/Src/CAPI/CAPI_HMDState.cpp \
				$(LIBOVRPATH)/Src/CAPI/CAPI_LatencyStatistics.cpp \
				$(LIBOVRPATH)/Src/CAPI/GL/CAPI_GL_DistortionRenderer.cpp \
				$(LIBOVRPATH)/Src/CAPI/GL/CAPI_GL_HSWDisplay.cpp \
				$(LIBOVRPATH)/Src/CAPI/GL/CAPI_GL_Util.cpp \
				$(LIBOVRPATH)/Src/CAPI/GL/CAPI_GLE.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_Alg.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_Allocator.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_Atomic.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_CRC32.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_File.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_FileFILE.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_Lockless.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_Log.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_Math.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_RefCount.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_SharedMemory.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_Std.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_String.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_String_FormatUtil.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_String_PathUtil.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_SysFile.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_System.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_ThreadsPthread.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_ThreadCommandQueue.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_Timer.cpp \
				$(LIBOVRPATH)/Src/Kernel/OVR_UTF8Util.cpp \
				$(LIBOVRPATH)/Src/Util/Util_Interface.cpp \
				$(LIBOVRPATH)/Src/Util/Util_LatencyTest2Reader.cpp \
				$(LIBOVRPATH)/Src/Util/Util_Render_Stereo.cpp \
				$(LIBOVRPATH)/Src/Util/Util_SystemInfo.cpp \
				$(LIBOVRPATH)/Src/OVR_CAPI.cpp \
				$(LIBOVRPATH)/Src/OVR_SerialFormat.cpp \
				$(LIBOVRPATH)/Src/OVR_JSON.cpp \
				$(LIBOVRPATH)/Src/OVR_Profile.cpp \
				$(LIBOVRPATH)/Src/OVR_Stereo.cpp

LIBOVR_OBJECTS = $(patsubst $(LIBOVRPATH)%.cpp,$(OBJPATH)%.o,$(LIBOVR_SOURCE))

OBJECTS = $(OTHER_OBJECTS) $(LIBOVR_OBJECTS)

$(OBJPATH)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

$(STATIC_TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	ar rvs $(STATIC_TARGET) $(OBJECTS)

$(SHARED_TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	g++ -shared -Wl,-soname,$(SHARED_TARGET).$(SHARED_MAJOR) \
		-o $(SHARED_TARGET).$(SHARED_MAJOR).$(SHARED_MINOR).$(SHARED_RELEASE) \
		$(OBJECTS) -lc

clean:
	-$(DELETEFILE) $(OBJECTS)
	-$(DELETEFILE) $(STATIC_TARGET)
	-$(DELETEFILE) $(SHARED_TARGET)

