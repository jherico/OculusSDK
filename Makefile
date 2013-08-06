#############################################################################
#
# Filename    : Makefile
# Content     : Makefile for building linux libovr and OculusWorldDemo
# Created     : 2013
# Authors     : Simon Hallam and Peter Giokaris
# Copyright   : Copyright 2013 OculusVR, Inc. All Rights Reserved
# Instruction : The g++ compiler and stdndard lib packages need to be 
#               installed on the system.  Navigate in a shell to the 
#               directory where this Makefile is located and enter:
#
#               make                builds the release versions for the 
#                                   current architechture
#               make clean          delete intermediate release object files 
#                                   and library and executable
#               make DEBUG=1        builds the debug version for the current
#                                   architechture
#               make clean DEBUG=1  deletes intermediate debug object files 
#                                   and the library and executable
#
# Output      : Relative to the directory this Makefile lives in, libraries
#               and executables are built at the following locations 
#               depending upon the architechture of the system you are 
#               running:
#
#           ./LibOVR/Lib/Linux/Debug/i386/libovr.a
#           ./LibOVR/Lib/Linux/Debug/x86_64/libovr.a
#           ./LibOVR/Lib/Linux/Release/i386/libovr.a
#           ./LibOVR/Lib/Linux/Release/x86_64/libovr.a
#           ./Samples/OculusWorldDemo/Release/OculusWorldDemo_i386_Release
#           ./Samples/OculusWorldDemo/Release/OculusWorldDemo_x86_64_Release
#           ./Samples/OculusWorldDemo/Release/OculusWorldDemo_i386_Debug
#           ./Samples/OculusWorldDemo/Release/OculusWorldDemo_x86_64_Debug
#
#############################################################################

DEBUG=0

####### Detect system architecture

SYSARCH       = i386
ifeq ($(shell uname -m),x86_64)
SYSARCH       = x86_64
endif

####### Detect debug or release

ifeq ($(DEBUG), 1)
	RELEASETYPE   = Debug
else
	RELEASETYPE   = Release
endif

####### Paths

CUSTOM_PATH   = $(RELEASETYPE)/$(SYSARCH)

####### Paths

LIBOVRPATH    = ./LibOVR
DEMOPATH      = ./Samples/OculusWorldDemo

####### Files

LIBOVRTARGET  = $(LIBOVRPATH)/Lib/Linux/$(CUSTOM_PATH)/libovr.a
DEMOTARGET    = $(DEMOPATH)/Release/OculusWorldDemo_$(SYSARCH)_$(RELEASETYPE)

####### Rules

all:    $(LIBOVRTARGET) $(DEMOTARGET)

$(DEMOTARGET): $(DEMOPATH)/Makefile
	$(MAKE) -C $(DEMOPATH)/../CommonSrc
	$(MAKE) -C $(DEMOPATH) 

$(LIBOVRTARGET): $(LIBOVRPATH)/Projects/Linux/Makefile
	$(MAKE) -C $(LIBOVRPATH)/Projects/Linux

clean:
	$(MAKE) -C $(LIBOVRPATH)/Projects/Linux clean 
	$(MAKE) -C $(DEMOPATH)/../CommonSrc clean
	$(MAKE) -C $(DEMOPATH) clean 

