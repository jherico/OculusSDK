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

##### Build flags

DEBUG         = 0
OVR_ROOT      = ../../..

include $(OVR_ROOT)/LibOVR/Projects/Linux/Makefile.common

LINK          = $(LINK_ARCHIVE)
INCPATH       = $(LIBOVRINCPATH)
SRCPATH       = $(LIBOVRPATH)/Src
OBJPATH       = $(LIBOVRPATH)/Obj/Linux/$(CUSTOM_PATH)
TARGETPATH    = $(LIBOVRPATH)/Lib/Linux/$(CUSTOM_PATH)

CXXBUILD      = $(CXX) -c $(CXXFLAGS) $(INCPATH) -o $(OBJPATH)/

####### Files

TARGET        = $(TARGETPATH)/libovr.a

# Filter out Windows and OSX files
CPP_SRCS = $(filter-out \
       $(SRCPATH)/OVR_Win32%.cpp \
       ${SRCPATH}/OVR_OSX%.cpp \
       ${SRCPATH}/OVR_Posix%.cpp \
       ${SRCPATH}/Kernel/OVR_ThreadsWinAPI.cpp \
       , $(shell find ${SRCPATH} -name *.cpp)) \


OBJECTS = $(patsubst ${SRCPATH}/%.cpp, ${OBJPATH}/%.o, ${CPP_SRCS}) $(OBJPATH)/tinyxml2.o

DIRS = $(subst /,/,$(sort $(dir $(OBJECTS))))  $(TARGETPATH)

####### Rules

all:    $(TARGET)

$(TARGET):  $(TARGET_PATH) $(OBJECTS) $(LIBOVRTARGET_PATH)
	$(MD) -p $(DIRS)
	$(LINK) $(TARGET) $(OBJECTS)

$(OBJPATH):
	$(MD) -p $(DIRS)

$(OBJPATH)/%.o: $(SRCPATH)/%.cpp
	$(MD) -p $(DIRS)
	$(CXXBUILD) -o "$@" "$<"

$(OBJPATH)/tinyxml2.o: $(3RDPARTYPATH)/TinyXml/tinyxml2.cpp 
	$(CXXBUILD) -o "$@" "$<" 

clean:
	-$(DELETEFILE) $(OBJECTS)
	-$(DELETEFILE) $(TARGET)

