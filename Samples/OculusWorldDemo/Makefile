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
#               make                builds the release version for the 
#                                   current architecture
#               make clean          delete intermediate release object files 
#                                   and the executable file
#               make DEBUG=1        builds the debug version for the current
#                                   architecture
#               make clean DEBUG=1  deletes intermediate debug object files 
#                                   and the executable file
#
# Output      : Relative to the directory this Makefile lives in, executable
#               files get built at the following locations depending upon the
#               architecture of the system you are running:
#
#               ./Release/OculusWorldDemo_i386_Release
#               ./Release/OculusWorldDemo_x86_64_Release
#               ./Release/OculusWorldDemo_i386_Debug
#               ./Release/OculusWorldDemo_x86_64_Debug
#
#############################################################################

####### Detect system architecture

SYSARCH       = i386
ifeq ($(shell uname -m),x86_64)
SYSARCH       = x86_64
endif

####### Compiler, tools and options

CXX           = g++
LINK          = g++
MAKE          = make
DELETEFILE    = rm -f
DELETEDIR     = rm -Rf
DEFINES       = -DQT_WEBKIT -DGL_GLEXT_PROTOTYPES
COMMONFLAGS   = -Wall -Wextra -Werror -pipe -msse2

####### Detect debug or release

DEBUG         ?= 0
CXXFLAGS 		  ?=
ifeq ($(DEBUG), 1)
	CXXFLAGS      += $(COMMONFLAGS) -DDEBUG -DOVR_BUILD_DEBUG -g $(DEFINES)
	RELEASETYPE   ?= Debug
	LFLAGS         = 
else
	CXXFLAGS      += $(COMMONFLAGS) -O2 $(DEFINES)
	RELEASETYPE   ?= Release
	LFLAGS         = -O1  # Why O1?
endif

####### Paths

OCULUSWORLDPATH = .
LIBOVRPATH      = ../../LibOVR
COMMONSRCPATH   = ../CommonSrc
3RDPARTYPATH    = ../../3rdParty
INCPATH         = -I. -I.. -I$(COMMONSRCPATH) -I$(LIBOVRPATH)/Include -I$(LIBOVRPATH)/Src
OBJPATH         = ./Obj/Linux/$(RELEASETYPE)/$(SYSARCH)
CXX_BUILD       = $(CXX) -c $(CXXFLAGS) $(INCPATH) -o $(OBJPATH)/

####### Files

LIBS          = -L$(LIBOVRPATH)/Lib/Linux/$(RELEASETYPE)/$(SYSARCH) \
		-lovr \
		-ludev \
		-lpthread \
		-lGL \
		-lX11 \
		-lXrandr \
		-lrt

TARGET        = ./Release/OculusWorldDemo_$(SYSARCH)_$(RELEASETYPE)

####### Rules

all:    $(TARGET)

OCULUSWORLD_SOURCE =	$(OCULUSWORLDPATH)/OculusWorldDemo.cpp \
						$(OCULUSWORLDPATH)/OculusWorldDemo_Scene.cpp \
						$(OCULUSWORLDPATH)/Player.cpp \
						$(OCULUSWORLDPATH)/../CommonSrc/Util/RenderProfiler.cpp \
						$(OCULUSWORLDPATH)/../CommonSrc/Util/OptionMenu.cpp \
						$(OCULUSWORLDPATH)/../CommonSrc/Platform/Linux_Gamepad.cpp \
						$(OCULUSWORLDPATH)/../CommonSrc/Platform/Linux_Platform.cpp \
						$(OCULUSWORLDPATH)/../CommonSrc/Platform/Platform.cpp \
						$(OCULUSWORLDPATH)/../CommonSrc/Render/Render_Device.cpp \
						$(OCULUSWORLDPATH)/../CommonSrc/Render/Render_GL_Device.cpp \
						$(OCULUSWORLDPATH)/../CommonSrc/Render/Render_LoadTextureDDS.cpp \
						$(OCULUSWORLDPATH)/../CommonSrc/Render/Render_LoadTextureTGA.cpp \
						$(OCULUSWORLDPATH)/../CommonSrc/Render/Render_XmlSceneLoader.cpp \
						$(OCULUSWORLDPATH)/../../3rdParty/TinyXml/tinyxml2.cpp

OCULUSWORLD_OBJECTS = $(patsubst $(OCULUSWORLDPATH)%.cpp,$(OBJPATH)%.o,$(OCULUSWORLD_SOURCE))

OBJECTS = $(OTHER_OBJECTS) $(OCULUSWORLD_OBJECTS)

$(OBJPATH)/%.o: %.cpp
	-mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

$(LIBOVRPATH)/Lib/Linux/$(RELEASETYPE)/$(SYSARCH)/libovr.a:
	$(MAKE) -C $(LIBOVRPATH) DEBUG=$(DEBUG)

lib: $(LIBOVRPATH)/Lib/Linux/$(RELEASETYPE)/$(SYSARCH)/libovr.a

run: $(TARGET)
	$(TARGET)

$(TARGET):  $(OBJECTS) lib
	-mkdir -p $(dir $@)
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

clean:
	-$(DELETEFILE) $(OBJECTS)
	-$(DELETEFILE) $(TARGET)

cleanall:
	-$(DELETEFILE) $(OBJECTS)
	-$(DELETEDIR) ./Release/*
