#############################################################################
#
# Filename    : Makefile
# Content     : Makefile for building linux OculusWorldDemo
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
#                                   and the executabe file
#               make DEBUG=1        builds the debug version for the current
#                                   architechture
#               make clean DEBUG=1  deletes intermediate debug object files 
#                                   and the executable file
#
# Output      : Relative to the directory this Makefile lives in, executable
#               files get built at the following locations depending upon the
#               architechture of the system you are running:
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
DEFINES       = -DQT_WEBKIT -DGL_GLEXT_PROTOTYPES

####### Detect debug or release

DEBUG         = 0
ifeq ($(DEBUG), 1)
	CXXFLAGS      = -pipe -DDEBUG -g $(DEFINES)
	LFLAGS        = 
	RELEASETYPE   = Debug
else
	CXXFLAGS      = -pipe -O2 $(DEFINES)
	LFLAGS        = -O1
	RELEASETYPE   = Release
endif

####### Paths

LIBOVRPATH    = ../../LibOVR
COMMONSRCPATH = ../CommonSrc
3RDPARTYPATH  = ../../3rdParty
INCPATH       = -I. -I.. -I$(COMMONSRCPATH) -I$(LIBOVRPATH)/Include -I$(LIBOVRPATH)/Src
OBJPATH       = ./Obj/Linux/$(RELEASETYPE)/$(SYSARCH)
CXX_BUILD     = $(CXX) -c $(CXXFLAGS) $(INCPATH) -o $(OBJPATH)/

####### Files

LIBS          = -L$(LIBOVRPATH)/Lib/Linux/$(RELEASETYPE)/$(SYSARCH) \
		-lovr \
		-ludev \
		-lpthread \
		-lGL \
		-lX11 \
		-lXinerama

OBJECTS       = $(OBJPATH)/OculusWorldDemo.o \
		$(OBJPATH)/Player.o \
		$(OBJPATH)/Platform.o \
		$(OBJPATH)/Linux_Platform.o \
		$(OBJPATH)/Linux_Gamepad.o \
		$(OBJPATH)/Render_Device.o \
		$(OBJPATH)/Render_GL_Device.o \
		$(OBJPATH)/Render_LoadTextureDDS.o \
		$(OBJPATH)/Render_LoadTextureTGA.o \
		$(OBJPATH)/Render_XmlSceneLoader.o 

TARGET        = ./Release/OculusWorldDemo_$(SYSARCH)_$(RELEASETYPE)

####### Rules

all:    $(TARGET)

$(TARGET):  $(LIBOVRPATH)/Lib/Linux/$(RELEASETYPE)/$(SYSARCH)/libovr.a
	$(MAKE) -C $(LIBOVRPATH) DEBUG=$(DEBUG)

$(TARGET):  $(OBJECTS)
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

$(OBJPATH)/OculusWorldDemo.o: OculusWorldDemo.cpp 
	$(CXX_BUILD)OculusWorldDemo.o OculusWorldDemo.cpp

$(OBJPATH)/Player.o: Player.cpp 
	$(CXX_BUILD)Player.o Player.cpp

$(OBJPATH)/Platform.o: ../../Samples/CommonSrc/Platform/Platform.cpp 
	$(CXX_BUILD)Platform.o ../../Samples/CommonSrc/Platform/Platform.cpp

$(OBJPATH)/Linux_Platform.o: ../../Samples/CommonSrc/Platform/Linux_Platform.cpp 
	$(CXX_BUILD)Linux_Platform.o ../../Samples/CommonSrc/Platform/Linux_Platform.cpp

$(OBJPATH)/Linux_Gamepad.o: ../../Samples/CommonSrc/Platform/Linux_Gamepad.cpp 
	$(CXX_BUILD)Linux_Gamepad.o ../../Samples/CommonSrc/Platform/Linux_Gamepad.cpp

$(OBJPATH)/Render_Device.o: ../../Samples/CommonSrc/Render/Render_Device.cpp $
	$(CXX_BUILD)Render_Device.o ../../Samples/CommonSrc/Render/Render_Device.cpp

$(OBJPATH)/Render_GL_Device.o: ../../Samples/CommonSrc/Render/Render_GL_Device.cpp 
	$(CXX_BUILD)Render_GL_Device.o ../../Samples/CommonSrc/Render/Render_GL_Device.cpp

$(OBJPATH)/Render_LoadTextureDDS.o: ../../Samples/CommonSrc/Render/Render_LoadTextureDDS.cpp 
	$(CXX_BUILD)Render_LoadTextureDDS.o ../../Samples/CommonSrc/Render/Render_LoadTextureDDS.cpp

$(OBJPATH)/Render_LoadTextureTGA.o: ../../Samples/CommonSrc/Render/Render_LoadTextureTGA.cpp 
	$(CXX_BUILD)Render_LoadTextureTGA.o ../../Samples/CommonSrc/Render/Render_LoadTextureTGA.cpp

$(OBJPATH)/Render_XmlSceneLoader.o: ../../Samples/CommonSrc/Render/Render_XmlSceneLoader.cpp 
	$(CXX_BUILD)Render_XmlSceneLoader.o ../../Samples/CommonSrc/Render/Render_XmlSceneLoader.cpp

clean:
	-$(DELETEFILE) $(OBJECTS)
	-$(DELETEFILE) $(TARGET)

