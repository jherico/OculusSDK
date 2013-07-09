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
OVR_ROOT      = ../..
SAMPLE_NAME	  = OculusWorldDemo
DEFINES       = -DQT_WEBKIT -DGL_GLEXT_PROTOTYPES

include $(OVR_ROOT)/LibOVR/Projects/Linux/Makefile.common
include $(OVR_ROOT)/Samples/Makefile.common

####### Compiler, tools and options

LINK          = $(LINK_EXE)
SRCPATH	      = .
INCPATH       = -I$(COMMONPATH) $(LIBOVRINCPATH)
LFLAGS        = -O1 -L$(TARGETPATH) -L$(LIBOVRLIBPATH) 
CXXBUILD      = $(CXX) -c $(CXXFLAGS) $(INCPATH) -o $(OBJPATH)/

####### Files

CPP_SRCS = $(shell find . -name "*.cpp")
OBJECTS = $(patsubst ${SRCPATH}/%.cpp, ${OBJPATH}/%.o, ${CPP_SRCS})
DIRS = $(subst /,/,$(sort $(dir $(OBJECTS)))) $(TARGETPATH)

####### Files

TARGET        = $(TARGETPATH)/OculusWorldDemo

####### Rules

all: $(TARGET)

$(TARGET):  $(OBJECTS)
	mkdir -p $(DIRS)
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

$(OBJPATH)/%.o: $(SRCPATH)/%.cpp 
	mkdir -p $(DIRS)
	$(CXXBUILD) -o "$@" "$<"

clean:
	-$(DELETEFILE) $(OBJECTS)
	-$(DELETEFILE) $(TARGET)

