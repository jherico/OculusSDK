#############################################################################
#
# Filename    : Makefile
# Content     : Makefile for building Linux version of: LibOVR
# Created     : 2014
# Copyright   : Copyright 2014 OculusVR, Inc. All Rights Reserved
# Instruction : The g++ compiler and standard lib packages need to be
#               installed on the system.  Navigate in a shell to the
#               directory where this Makefile is located and enter:
#
# make targets:
#               all
#               clean
#
# environment variable options:
#               DEBUG = [0|1]
#               SYSARCH = (system touple, like: "x86_64_linux_gnu")
#               SINGLEPROCESS = [0|1]
#               STATIC = [0|1]          Indicates a static library as opposed to shared libary.
#
# Output      : Relative to the directory this makefile lives in, libraries
#               are built at the following locations depending upon the
#               architecture of the system you are running:
#
#               ../../Lib/Linux/i386/Debug/libOVRRT.so
#               ../../Lib/Linux/i386/DebugStatic/libOVRRT.a
#               ../../Lib/Linux/i386/DebugSingleProcess/libOVRRT.so
#               ../../Lib/Linux/i386/DebugSingleProcessStatic/libOVRRT.a
#               (etc. for x86_64 and Release)
#############################################################################

####### Include auxiliary makefiles in current directory
# Exports SYSARCH, SYSSIZE, PREPROCDEFS, RELEASESUFFIX, RELEASETYPE
THIS_MAKE_DIR:= $(dir $(lastword $(MAKEFILE_LIST)))

include $(THIS_MAKE_DIR)LibOVRCommon.mk

####### Compiler options
# By default, builds are release builds. They are debug builds if DEBUG=1 is set.
# Static library or shared library? Default is shared, but can use STATIC=1 environment to specify a static library.
# Disable strict aliasing due to the List class violating strict aliasing requirements.
STATIC        ?= 0
DEBUG         ?= 0
COMMONFLAGS    = -Wall -Wextra -Werror -pipe -fPIC -msse2 -fno-strict-aliasing -fvisibility=hidden -std=c++11
CXXFLAGS      ?=
CXXFLAGS      += $(PREPROCDEFS)

ifeq ($(DEBUG), 1)
	CXXFLAGS += $(COMMONFLAGS) -DDEBUG -DOVR_BUILD_DEBUG -g -O0
else
	CXXFLAGS += $(COMMONFLAGS) -O2
endif

####### Paths
LIBOVRKERNEL_PATH   = ../../../LibOVRKernel
LIBOVRKERNEL_TARGET = $(LIBOVRKERNEL_PATH)/Lib/Linux/$(SYSARCH)/$(RELEASETYPE)/libOVRKernel.a

LIBOVRRT_PATH      = ../..
3RDPARTY_PATH      = ../../../3rdParty
INC_PATH           = -I$(LIBOVRRT_PATH)/Include -I$(LIBOVRRT_PATH)/Src -I$(LIBOVRKERNEL_PATH)/Src -I$(LIBOVRKERNEL_PATH)/Src/Kernel
OBJ_PATH           = ../../Obj/Linux/$(SYSARCH)/$(RELEASETYPE)

####### Files
TARGET_DIR               = ../../Lib/Linux/$(SYSARCH)/$(RELEASETYPE)
STATIC_BASE_FILE_NAME    = libOVRRT.a
STATIC_TARGET            = $(TARGET_DIR)/$(STATIC_BASE_FILE_NAME)
SO_BASE_FILE_NAME        = libOVRRT$(SYSSIZE)_$(OVR_PRODUCT_VERSION).so
SO_MAJOR_FILE_NAME       = libOVRRT$(SYSSIZE)_$(OVR_PRODUCT_VERSION).so.$(OVR_MAJOR_VERSION)
SO_FULL_FILE_NAME        = libOVRRT$(SYSSIZE)_$(OVR_PRODUCT_VERSION).so.$(OVR_MAJOR_VERSION).$(OVR_MINOR_VERSION).$(OVR_PATCH_VERSION)
SO_MAJOR_FILE_PATH       = $(TARGET_DIR)/$(SO_MAJOR_FILE_NAME)
SO_FULL_FILE_PATH        = $(TARGET_DIR)/$(SO_FULL_FILE_NAME)
SO_TARGET                = $(SO_FULL_FILE_PATH)

####### Rules
# Note that we have independent static and SO targets and we don't build the SO target as
# something that simply links an always-built static target. The reason for this is that we 
# need different symbol visibility for static vs shared libraries. The static library needs
# to export (make visible) all symbols, whereas the shared needs to export only specific symbols.
# This partly derives from our secrecy goals, the value of which is a separate discussion.
ifeq ($(STATIC), 1)
TARGET = $(STATIC_TARGET)
else
TARGET = $(SO_TARGET)
endif

all: $(TARGET)

LIBOVR_SOURCE =	$(3RDPARTY_PATH)/EDID/edid.cpp \
		$(LIBOVRRT_PATH)/Src/Net/OVR_BitStream.cpp \
		$(LIBOVRRT_PATH)/Src/Net/OVR_NetworkPlugin.cpp \
		$(LIBOVRRT_PATH)/Src/Net/OVR_PacketizedTCPSocket.cpp \
		$(LIBOVRRT_PATH)/Src/Net/OVR_RPC1.cpp \
		$(LIBOVRRT_PATH)/Src/Net/OVR_Session.cpp \
		$(LIBOVRRT_PATH)/Src/Net/OVR_Socket.cpp \
		$(LIBOVRRT_PATH)/Src/Net/OVR_Unix_Socket.cpp \
		$(LIBOVRRT_PATH)/Src/Service/Service_NetClient.cpp \
		$(LIBOVRRT_PATH)/Src/Service/Service_NetSessionCommon.cpp \
		$(LIBOVRRT_PATH)/Src/Displays/OVR_Display.cpp \
		$(LIBOVRRT_PATH)/Src/Displays/OVR_Linux_Display.cpp \
		$(LIBOVRRT_PATH)/Src/Displays/OVR_Linux_SDKWindow.cpp \
		$(LIBOVRRT_PATH)/Src/CAPI/CAPI_DistortionRenderer.cpp \
		$(LIBOVRRT_PATH)/Src/CAPI/CAPI_DistortionTiming.cpp \
		$(LIBOVRRT_PATH)/Src/CAPI/CAPI_FrameLatencyTracker.cpp \
		$(LIBOVRRT_PATH)/Src/CAPI/CAPI_FrameTimeManager3.cpp \
		$(LIBOVRRT_PATH)/Src/CAPI/CAPI_HMDRenderState.cpp \
		$(LIBOVRRT_PATH)/Src/CAPI/CAPI_HMDState.cpp \
		$(LIBOVRRT_PATH)/Src/CAPI/CAPI_HSWDisplay.cpp \
		$(LIBOVRRT_PATH)/Src/CAPI/GL/CAPI_GL_DistortionRenderer.cpp \
		$(LIBOVRRT_PATH)/Src/CAPI/GL/CAPI_GL_HSWDisplay.cpp \
		$(LIBOVRRT_PATH)/Src/CAPI/GL/CAPI_GL_Util.cpp \
		$(LIBOVRRT_PATH)/Src/Util/Util_Interface.cpp \
		$(LIBOVRRT_PATH)/Src/Util/Util_LatencyTest2Reader.cpp \
		$(LIBOVRRT_PATH)/Src/Util/Util_Render_Stereo.cpp \
		$(LIBOVRRT_PATH)/Src/Vision/SensorFusion/Vision_SensorStateReader.cpp \
		$(LIBOVRRT_PATH)/Src/OVR_CAPI.cpp \
		$(LIBOVRRT_PATH)/Src/OVR_CAPI_Util.cpp \
		$(LIBOVRRT_PATH)/Src/OVR_Linux_UDEV.cpp \
		$(LIBOVRRT_PATH)/Src/OVR_Profile.cpp \
		$(LIBOVRRT_PATH)/Src/OVR_SerialFormat.cpp \
		$(LIBOVRRT_PATH)/Src/OVR_Stereo.cpp \
		$(LIBOVRRT_PATH)/Src/OVR_StereoProjection.cpp


LIBOVR_OBJECTS = $(patsubst $(LIBOVRRT_PATH)%.cpp,$(OBJ_PATH)%.o,$(LIBOVR_SOURCE))

OBJECTS = $(LIBOVR_OBJECTS)

####### Library references
LDLIBS += -L$(LIBOVRKERNEL_PATH)/Lib/Linux/$(SYSARCH)/$(RELEASETYPE) \
          -lOVRKernel \
          -lpthread \
          -lGL \
          -lXrandr \
          -lrt \
          -lc \

####### Target definitions
FORCE:

$(LIBOVRKERNEL_TARGET): FORCE
	$(MAKE) -C $(LIBOVRKERNEL_PATH)/Projects/Linux SINGLEPROCESS=$(SINGLEPROCESS) DEBUG=$(DEBUG) STATIC=$(STATIC) --file LibOVRKernel.mk

$(OBJ_PATH)%.o: $(LIBOVRRT_PATH)%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(INC_PATH) -o $@ $<

$(OBJ_PATH)%.o: $(LIBOVRRT_PATH)%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(INC_PATH) -o $@ $<

$(OBJ_PATH)%.d: $(LIBOVRRT_PATH)%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INC_PATH) -MM $< > $@

$(OBJ_PATH)%.d: $(LIBOVRRT_PATH)%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INC_PATH) -MM $< > $@

$(STATIC_TARGET): $(OBJECTS) $(LIBOVRKERNEL_TARGET)
	@mkdir -p $(@D)
	ar rvs $(STATIC_TARGET) $(OBJECTS)

# We create a .so with the full file name (e.g. /somepath/LibOVRRT.so.1.3.0) but make a 
# symlink to it with just the major file name (e.g. /somepath/LibOVRRT.so.1). This is because
# at runtime our code looks only for the major file name and we are not getting the ldconfig
# system involved here.
$(SO_TARGET): $(OBJECTS) $(LIBOVRKERNEL_TARGET)
	@mkdir -p $(@D)
	ln --force -s $(SO_FULL_FILE_NAME) $(SO_MAJOR_FILE_PATH)
	$(CXX) -shared -Wl,-soname,$(SO_FULL_FILE_NAME) -o $@ $(OBJECTS) $(LDLIBS)

clean: FORCE
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*/*/*/*/*.[od]
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*/*/*/*.[od]
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*/*/*.[od]
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*/*.[od]
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*.[od]
	@$(DELETEFILE) ../../Obj/Linux/*/*/*.[od]
	@$(DELETEDIR) ../../Obj/Linux/*/*/*/*/*/*/*
	@$(DELETEDIR) ../../Obj/Linux/*/*/*/*/*/*
	@$(DELETEDIR) ../../Obj/Linux/*/*/*/*/*
	@$(DELETEDIR) ../../Obj/Linux/*/*/*/*
	@$(DELETEDIR) ../../Obj/Linux/*/*/*
	@$(DELETEDIR) ../../Obj/Linux/*/*
	@$(DELETEDIR) ../../Obj/Linux/*
	@$(DELETEDIR) ../../Obj/Linux
	@$(DELETEDIR) ../../Obj
	@$(DELETEFILE) ../../Lib/Linux/*/*/*.so.*
	@$(DELETEFILE) ../../Lib/Linux/*/*/*.so
	@$(DELETEFILE) ../../Lib/Linux/*/*/*.a
	@$(DELETEDIR) ../../Lib/Linux/*/*
	@$(DELETEDIR) ../../Lib/Linux/*
	@$(DELETEDIR) ../../Lib/Linux
	@$(DELETEDIR) ../../Lib

-include $(OBJECTS:.o=.d)
