#############################################################################
#
# Filename    : LibOVR.mk
# Content     : Makefile for building Linux version of: LibOVR
# Created     : 2014
# Copyright   : Copyright 2014 OculusVR, Inc. All Rights Reserved
# Instruction : The g++ compiler and standard lib packages need to be
#               installed on the system. Navigate in a shell to the
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
#
# Output      : Relative to the directory this makefile lives in, libraries
#               are built at the following locations depending upon the
#               architecture of the system you are running:
#
#               ../../Lib/Linux/i386/Debug/libOVR.a
#               ../../Lib/Linux/i386/DebugSingleProcess/libOVR.a
#               (etc. for x86_64 and Release)
#
#############################################################################

####### Include auxiliary makefiles in current directory
# Exports SYSARCH, PREPROCDEFS, RELEASESUFFIX, RELEASETYPE
include LibOVRCommon.mk

####### Compiler options
# By default, builds are release builds. They are debug builds if DEBUG=1 is set.
DEBUG       ?= 0
COMMONFLAGS  = -Wall -Wextra -Werror -pipe -fPIC -msse2
CFLAGS      ?=
CFLAGS      += $(PREPROCDEFS)
CXXFLAGS    ?=
CXXFLAGS    += $(PREPROCDEFS)
ifeq ($(DEBUG), 1)
	CFLAGS += $(COMMONFLAGS) -DDEBUG -DOVR_BUILD_DEBUG -g -O0
	CXXFLAGS += $(COMMONFLAGS) -DDEBUG -DOVR_BUILD_DEBUG -g -O0
else
	CFLAGS += $(COMMONFLAGS) -O2
	CXXFLAGS += $(COMMONFLAGS) -O2
endif

####### Version numbering
include ../../../LibOVR/Projects/Linux/LibOVRConfig.mk

####### Paths
LIBOVRRT_PATH   = ../../../LibOVR
LIBOVRRT_TARGET = $(LIBOVRRT_PATH)/Obj/Linux/$(SYSARCH)/$(RELEASETYPE)/libOVRRT_$(OVR_PRODUCT_VERSION).so.$(OVR_MAJOR_VERSION)

LIBOVR_PATH     = ../..
INC_PATH         = -I$(LIBOVR_PATH)/Include
OBJ_PATH       = ../../Obj/Linux/$(SYSARCH)/$(RELEASETYPE)

####### Files
TARGET_DIR      = ../../Lib/Linux/$(SYSARCH)/$(RELEASETYPE)
STATIC_TARGET   = $(TARGET_DIR)/libOVR.a

####### Rules
# We currently do not make LibOVR.so here, and instead assume that it's already made.
# We do this because we don't link LibOVR.so at build time.
all: $(LIBOVRRT_TARGET) $(STATIC_TARGET)

LIBOVRSHIM_C_SOURCE = $(LIBOVR_PATH)/Src/OVR_CAPIShim.c
LIBOVRSHIM_CPP_SOURCE = $(LIBOVR_PATH)/Src/OVR_CAPI_Util.cpp \
	$(LIBOVR_PATH)/Src/OVR_StereoProjection.cpp

LIBOVRSHIM_C_OBJECTS = $(patsubst $(LIBOVR_PATH)%.c,$(OBJ_PATH)%.o,$(LIBOVRSHIM_C_SOURCE))
LIBOVRSHIM_CPP_OBJECTS = $(patsubst $(LIBOVR_PATH)%.cpp,$(OBJ_PATH)%.o,$(LIBOVRSHIM_CPP_SOURCE))

OBJECTS = $(LIBOVRSHIM_C_OBJECTS) $(LIBOVRSHIM_CPP_OBJECTS)

FORCE:

$(LIBOVRRT_TARGET): FORCE
	$(MAKE) -C $(LIBOVRRT_PATH)/Projects/Linux SINGLEPROCESS=$(SINGLEPROCESS) DEBUG=$(DEBUG) STATIC=$(STATIC) --file LibOVRRT.mk

$(OBJ_PATH)%.o: $(LIBOVR_PATH)%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(INC_PATH) -o $@ $<

$(OBJ_PATH)%.o: $(LIBOVR_PATH)%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(INC_PATH) -o $@ $<

$(OBJ_PATH)%.d: $(LIBOVR_PATH)%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INC_PATH) -MM $< > $@

$(OBJ_PATH)%.d: $(LIBOVR_PATH)%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INC_PATH) -MM $< > $@

$(STATIC_TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	ar rvs $(STATIC_TARGET) $(OBJECTS)

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
