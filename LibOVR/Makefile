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

include ../Makefile.config
include ../Makefile.include

ifeq ($(DEBUG), 1)
	CXXFLAGS      = -pipe -DDEBUG -g
else
	CXXFLAGS      = -pipe -O2
endif

####### Paths

LIBOVRPATH    = .
3RDPARTYPATH  = ../3rdParty
INCPATH       = -I./Include -I./Src
SRCPATH       = ./Src
OBJPATH       = ./Obj/Linux/$(CUSTOM_PATH)
CXXBUILD      = $(CXX) -c $(CXXFLAGS) $(INCPATH) -o $(OBJPATH)/

TARGET_PATH   = ./Lib/Linux/$(CUSTOM_PATH)


# Filter out Windows and OSX files
CPP_SRCS = $(filter-out \
       $(SRCPATH)/OVR_Win32%.cpp \
       ${SRCPATH}/OVR_OSX%.cpp \
       ${SRCPATH}/Kernel/OVR_ThreadsWinAPI.cpp \
       , $(shell find ${SRCPATH} -name *.cpp)) \

####### Files

TARGET        = $(TARGET_PATH)/libovr.a

OBJECTS = $(patsubst ${SRCPATH}/%.cpp, ${OBJPATH}/%.o, ${CPP_SRCS}) $(OBJPATH)/tinyxml2.o
DIRS = $(subst /,/,$(sort $(dir $(OBJECTS))))

####### Rules

all:    $(TARGET)

$(TARGET):  $(TARGET_PATH) $(OBJECTS) $(LIBOVRTARGET_PATH)
	$(LINK) $(TARGET) $(OBJECTS)

$(TARGET_PATH): $(OBJPATH)
	$(MD) -p $(TARGET_PATH)

$(OBJPATH):
	$(MD) -p $(DIRS)

$(OBJPATH)/%.o: $(SRCPATH)/%.cpp
	$(CXXBUILD) -o "$@" "$<"

$(OBJPATH)/tinyxml2.o: $(3RDPARTYPATH)/TinyXml/tinyxml2.cpp 
	$(CXXBUILD) -o "$@" "$<" 

clean:
	-$(DELETEFILE) $(OBJECTS)
	-$(DELETEFILE) $(TARGET)

