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

##### Build flags

DEBUG         = 0

####### Compiler, tools and options

CXX           = g++
LINK          = g++
MAKE          = make
DELETEFILE    = rm -f
DEFINES       = -DQT_WEBKIT -DGL_GLEXT_PROTOTYPES
MD            = mkdir

####### Detect system architecture

SYSARCH       = i386
ifeq ($(shell uname -m),x86_64)
SYSARCH       = x86_64
endif

####### Paths

CUSTOM_PATH   = $(RELEASETYPE)/$(SYSARCH)


####### Detect debug or release

ifeq ($(DEBUG), 1)
	CXXFLAGS      = -pipe -DDEBUG -g $(DEFINES)
	RELEASETYPE   = Debug
else
	CXXFLAGS      = -pipe -O2 $(DEFINES)
	RELEASETYPE   = Release
endif

####### Compiler, tools and options

LIBOVRPATH    = ../../LibOVR
SRCPATH	      = .
COMMONSRCPATH = ../CommonSrc
3RDPARTYPATH  = ../../3rdParty
INCPATH       = -I. -I.. -I$(COMMONSRCPATH) -I$(LIBOVRPATH)/Include -I$(LIBOVRPATH)/Src
OBJPATH       = ./Obj/Linux/$(RELEASETYPE)/$(SYSARCH)
LFLAGS        = -O1 -L$(LIBOVRPATH)/Lib/Linux/$(RELEASETYPE)/$(SYSARCH)
LIBS          = $(SUBLIBS) -lovr -ludev -lpthread -lGL -lX11 -lXinerama
CXX_BUILD     = $(CXX) -c $(CXXFLAGS) $(INCPATH) -o $(OBJPATH)/

####### Files

# Filter out Windows and OSX files
COMMON_SRCPATH = ./Common

ALL_SRCS := $(shell find ${COMMONSRCPATH} -name "*.cpp")
ALL_SRCS += $(shell find . -name "*.cpp") 

CPP_SRCS = $(filter-out \
       $(COMMONSRCPATH)/Render/Render_D3D%.cpp \
       $(COMMONSRCPATH)/Render/Render_GL_GLUT%.cpp \
       $(COMMONSRCPATH)/Render/Render_GL_Win32%.cpp \
       $(COMMONSRCPATH)/Render/Render_SDL%.cpp \
       $(COMMONSRCPATH)/Platform/OSX_%.cpp \
       $(COMMONSRCPATH)/Platform/Win32_%.cpp \
       $(COMMONSRCPATH)/Platform/SDL_%.cpp \
       $(COMMONSRCPATH)/Platform/GLUT_%.cpp \
       , $(ALL_SRCS))

OBJECTS_1 = $(patsubst ${SRCPATH}/%.cpp, ${OBJPATH}/%.o, ${CPP_SRCS})
OBJECTS = $(patsubst ${COMMONSRCPATH}/%.cpp, ${OBJPATH}/%.o, ${OBJECTS_1})

DIRS = $(subst /,/,$(sort $(dir $(OBJECTS)))) ./Release

####### Files

TARGET        = ./Release/OculusWorldDemo_$(SYSARCH)_$(RELEASETYPE)

####### Rules

all: $(TARGET)

$(TARGET):  $(OBJECTS)
	mkdir -p $(DIRS)
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

$(OBJPATH)/%.o: $(COMMONSRCPATH)/%.cpp
	mkdir -p $(DIRS)
	$(CXX_BUILD) -o "$@" "$<" 

$(OBJPATH)/%.o: $(SRCPATH)/%.cpp 
	mkdir -p $(DIRS)
	$(CXX_BUILD) -o "$@" "$<"

clean:
	-$(DELETEFILE) $(OBJECTS)
	-$(DELETEFILE) $(TARGET)

