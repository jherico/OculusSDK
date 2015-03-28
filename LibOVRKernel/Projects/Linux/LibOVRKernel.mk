#############################################################################
#
# Filename    : LibOVRKernel.mk
# Content     : Makefile for building Linux version of: LibOVRKernel
# Created     : 2014
# Authors     : Simon Hallam, Peter Giokaris, Chris Taylor
# Copyright   : Copyright 2014 OculusVR, Inc. All Rights Reserved
# Instruction : The g++ compiler and standard lib packages need to be
#               installed on the system. Navigate in a shell to the
#               directory where this Makefile is located and enter:
#
#               make                builds the release version for the
#                                   current architecture
#               make clean          delete intermediate release object files
#                                   and the library file
#               make DEBUG=1        builds the debug version for the current
#                                   architecture
#               make clean DEBUG=1  deletes intermediate debug object files
#                                   and the library file
#               In addition to DEGUG=0|1, you can also use SINGLEPROCESS=0|1, 
#               with the meaning of which is intrinsic to LibOVR.
#
# Output      : Relative to the directory this makefile lives in, libraries
#               are built at the following locations depending upon the
#               architecture of the system you are running:
#
#               ../../Lib/Linux/i386/Debug/libOVRKernel.a
#               ../../Lib/Linux/i386/DebugSingleProcess/libOVRKernel.a
#               (etc. for x86_64 and Release)
#
#############################################################################

THIS_MAKE_DIR:= $(dir $(lastword $(MAKEFILE_LIST)))

####### Include auxiliary makefiles in current directory
# Exports SYSARCH, PREPROCDEFS, RELEASESUFFIX, RELEASETYPE
include $(THIS_MAKE_DIR)../../../LibOVR/Projects/Linux/LibOVRCommon.mk

####### Compiler options
# By default, builds are release builds. They are debug builds if DEBUG=1 is set.
# Disable strict aliasing due to the List class violating strict aliasing requirements.
# Set visibility as hidden because we don't want symbol collisions when this library is used by multiple modules.
DEBUG         ?= 0
COMMONFLAGS    = -Wall -Wextra -Wshadow -Werror -pipe -fPIC -msse2 -fno-strict-aliasing -fvisibility=hidden -std=c++11
CXXFLAGS      ?=
CXXFLAGS      += $(PREPROCDEFS)

ifeq ($(DEBUG), 1)
	CXXFLAGS += $(COMMONFLAGS) -DDEBUG -DOVR_BUILD_DEBUG -g -O0
else
	CXXFLAGS += $(COMMONFLAGS) -O2
endif

####### Paths

LIBOVRKERNEL_PATH    = ../..
INC_PATH             = -I$(LIBOVRKERNEL_PATH)/Src
OBJ_PATH             = ../../Obj/Linux/$(SYSARCH)/$(RELEASETYPE)

####### Files

TARGET_DIR      = ../../Lib/Linux/$(SYSARCH)/$(RELEASETYPE)
STATIC_TARGET   = $(TARGET_DIR)/libOVRKernel.a

####### Rules

all:    $(STATIC_TARGET)

skipwin = $(foreach v,$1,$(if $(findstring Win32,$v),,$v))
skiposx = $(foreach v,$1,$(if $(findstring OSX,$v),,$v))
skipand = $(foreach v,$1,$(if $(findstring Android,$v),,$v))
skipd3d = $(foreach v,$1,$(if $(findstring Direct3D,$v),,$v))
skipapi = $(foreach v,$1,$(if $(findstring WinAPI,$v),,$v))
skip = $(call skipapi,$(call skipd3d,$(call skipand,$(call skiposx,$(call skipwin,$1)))))
LIBOVRKERNEL_SOURCE =	$(call skip,$(wildcard $(LIBOVRKERNEL_PATH)/Src/Kernel/*.cpp)) \
			$(call skip,$(wildcard $(LIBOVRKERNEL_PATH)/Src/Util/*.cpp)) \
			$(call skip,$(wildcard $(LIBOVRKERNEL_PATH)/Src/GL/*.cpp))

LIBOVRKERNEL_OBJECTS = $(patsubst $(LIBOVRKERNEL_PATH)%.cpp,$(OBJ_PATH)%.o,$(LIBOVRKERNEL_SOURCE))

OBJECTS = $(LIBOVRKERNEL_OBJECTS)

$(OBJ_PATH)%.o: $(LIBOVRKERNEL_PATH)%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(INC_PATH) -o $@ $<

$(OBJ_PATH)%.d: $(LIBOVRKERNEL_PATH)%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INC_PATH) -MM $< > $@

$(STATIC_TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	ar rvs $(STATIC_TARGET) $(OBJECTS)

FORCE:

clean: FORCE
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*/*/*.[od]
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*/*.[od]
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*.[od]
	@$(DELETEFILE) ../../Obj/Linux/*/*/*.[od]
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
