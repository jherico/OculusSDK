#############################################################################
#
# Filename    : Makefile
# Content     : Makefile for building Linux OculusWorldDemo
# Created     : 2013
# Authors     : Simon Hallam and Peter Giokaris
# Copyright   : Copyright 2013 OculusVR, Inc. All Rights Reserved
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
#               SYSARCH = [i386|x86_64]
#
# Output      : Relative to the directory this Makefile lives in, executable
#               files get built at the following locations depending upon the
#               architecture of the system you are running:
#
#############################################################################

####### Include auxiliary makefiles in current directory
# Exports SYSARCH, PREPROCDEFS, RELEASESUFFIX, RELEASETYPE
include ../../../../LibOVR/Projects/Linux/LibOVRCommon.mk

####### Basic options
COMMONFLAGS   = -Wall -Wextra -Werror -pipe -fPIC -msse2 -fno-strict-aliasing -fvisibility=hidden -std=c++11

####### Detect debug or release
DEBUG        ?= 0
CXXFLAGS     ?=
CXXFLAGS     += $(PREPROCDEFS)
ifeq ($(DEBUG), 1)
	CXXFLAGS += $(COMMONFLAGS) -DDEBUG -DOVR_BUILD_DEBUG -g -O0
	LDFLAGS  +=
else
	CXXFLAGS += $(COMMONFLAGS) -O2
	LDFLAGS  += -O1
endif

####### Paths
LIBOVR_PATH          = ../../../../LibOVR
LIBOVR_TARGET        = $(LIBOVR_PATH)/Lib/Linux/$(SYSARCH)/$(RELEASETYPE)/libOVR.a

LIBOVRKERNEL_PATH    = ../../../../LibOVRKernel
LIBOVRKERNEL_TARGET  = $(LIBOVRKERNEL_PATH)/Lib/Linux/$(SYSARCH)/$(RELEASETYPE)/libOVRKernel.a

OCULUSWORLDDEMO_PATH = ../..
COMMONSRC_PATH       = ../../../CommonSrc
3RDPARTY_PATH        = ../../../../3rdParty
INC_PATH             = -I$(LIBOVR_PATH)/Include -I$(LIBOVRKERNEL_PATH)/Src
OBJ_PATH             = ../../Obj/Linux/$(SYSARCH)/$(RELEASETYPE)
CXX_BUILD            = $(CXX) -c $(CXXFLAGS) $(INC_PATH) -o $(OBJ_PATH)/

####### Libraries
LDLIBS += -L$(LIBOVR_PATH)/Lib/Linux/$(SYSARCH)/$(RELEASETYPE) \
          -L$(LIBOVRKERNEL_PATH)/Lib/Linux/$(SYSARCH)/$(RELEASETYPE) \
          -lOVR \
          -lOVRKernel \
          -lpthread \
          -lGL \
          -lX11 \
          -lXrandr \
          -ldl \
          -lrt \

# Source and objs
TARGET = ../../Bin/$(SYSARCH)/$(RELEASETYPE)/OculusWorldDemo

OCULUSWORLD_SOURCE  =	$(OCULUSWORLDDEMO_PATH)/OculusWorldDemo.cpp \
			$(OCULUSWORLDDEMO_PATH)/OculusWorldDemo_Scene.cpp \
			$(OCULUSWORLDDEMO_PATH)/Player.cpp
OCULUSWORLD_OBJECTS =   $(patsubst $(OCULUSWORLDDEMO_PATH)%.cpp,$(OBJ_PATH)%.o,$(OCULUSWORLD_SOURCE))

COMMONSRC_SOURCE    = 	$(COMMONSRC_PATH)/Util/RenderProfiler.cpp \
			$(COMMONSRC_PATH)/Util/OptionMenu.cpp \
			$(COMMONSRC_PATH)/Platform/Linux_Gamepad.cpp \
			$(COMMONSRC_PATH)/Platform/Linux_Platform.cpp \
			$(COMMONSRC_PATH)/Platform/Platform.cpp \
			$(COMMONSRC_PATH)/Render/Render_Device.cpp \
			$(COMMONSRC_PATH)/Render/Render_GL_Device.cpp \
			$(COMMONSRC_PATH)/Render/Render_LoadTextureDDS.cpp \
			$(COMMONSRC_PATH)/Render/Render_LoadTextureTGA.cpp \
			$(COMMONSRC_PATH)/Render/Render_XmlSceneLoader.cpp
COMMONSRC_OBJECTS   =   $(patsubst $(COMMONSRC_PATH)%.cpp,$(OBJ_PATH)%.o,$(COMMONSRC_SOURCE))

3RDPARTY_SOURCE     = 	$(3RDPARTY_PATH)/TinyXml/tinyxml2.cpp \
			$(3RDPARTY_PATH)/EDID/edid.cpp
3RDPARTY_OBJECTS   =   $(patsubst $(3RDPARTY_PATH)%.cpp,$(OBJ_PATH)%.o,$(3RDPARTY_SOURCE))


OBJECTS = $(OCULUSWORLD_OBJECTS) $(COMMONSRC_OBJECTS) $(3RDPARTY_OBJECTS)

####### Rules

all:    $(TARGET)

$(LIBOVR_TARGET): force_look
	$(MAKE) -C $(LIBOVR_PATH)/Projects/Linux SINGLEPROCESS=$(SINGLEPROCESS) DEBUG=$(DEBUG) STATIC=$(STATIC) --file LibOVR.mk

$(LIBOVRKERNEL_TARGET): force_look
	$(MAKE) -C $(LIBOVRKERNEL_PATH)/Projects/Linux SINGLEPROCESS=$(SINGLEPROCESS) DEBUG=$(DEBUG) STATIC=$(STATIC) --file LibOVRKernel.mk

force_look :
	true

# To consider: Find a way to merge the following obj rules.
$(OBJ_PATH)/%.o: $(OCULUSWORLDDEMO_PATH)/%.cpp
	-mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(INC_PATH) -o $@ $<

$(OBJ_PATH)/%.o: $(COMMONSRC_PATH)/%.cpp
	-mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(INC_PATH) -o $@ $<

$(OBJ_PATH)/%.o: $(3RDPARTY_PATH)/%.cpp
	-mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(INC_PATH) -o $@ $<

$(TARGET):  $(OBJECTS) $(LIBOVR_TARGET) $(LIBOVRKERNEL_TARGET)
	-mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(LDLIBS)

clean:
	$(MAKE) -C $(LIBOVR_PATH)/Projects/Linux --file LibOVR.mk clean
	$(MAKE) -C $(LIBOVRKERNEL_PATH)/Projects/Linux --file LibOVRKernel.mk clean
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*/*/*.o
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*/*.o
	@$(DELETEFILE) ../../Obj/Linux/*/*/*/*.o
	@$(DELETEFILE) ../../Obj/Linux/*/*/*.o
	@$(DELETEDIR) ../../Obj/Linux/*/*/*/*/*
	@$(DELETEDIR) ../../Obj/Linux/*/*/*/*
	@$(DELETEDIR) ../../Obj/Linux/*/*/*
	@$(DELETEDIR) ../../Obj/Linux/*/*
	@$(DELETEDIR) ../../Obj/Linux/*
	@$(DELETEDIR) ../../Obj/Linux
	@$(DELETEDIR) ../../Obj
	@$(DELETEFILE) ../../Bin/*/*/OculusWorldDemo
	@$(DELETEDIR) ../../Bin/*/*
	@$(DELETEDIR) ../../Bin/*
	@$(DELETEDIR) ../../Bin
