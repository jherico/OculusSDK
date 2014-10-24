#############################################################################
#
# Filename    : Makefile
# Content     : Makefile for building linux libovr and OculusWorldDemo
# Created     : 2013
# Authors     : Simon Hallam and Peter Giokaris
# Copyright   : Copyright 2013 OculusVR, Inc. All Rights Reserved
# Instruction : See 'make help'
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

####### Include makefiles in current directory
RELEASESUFFIX =
-include Makefile.*[^~]

####### Detect system architecture

SYSARCH       = i386
ifeq ($(shell uname -m),x86_64)
SYSARCH       = x86_64
endif

####### Compiler, tools and options

CXX           = g++
LINK          = ar rvs
DELETEFILE    = rm -f

####### Detect debug or release

DEBUG         = 0
ifeq ($(DEBUG), 1)
	RELEASETYPE   = Debug$(RELEASESUFFIX)
else
	RELEASETYPE   = Release$(RELEASESUFFIX)
endif

# Override release types and DEBUG settings in child makefiles.
export RELEASETYPE
export DEBUG

####### Target settings
LIBOVRPATH    = ./LibOVR
OWDPATH       = ./Samples/OculusWorldDemo

LIBOVRTARGET  = $(LIBOVRPATH)/Lib/Linux/$(RELEASETYPE)/$(SYSARCH)/libovr.a
OWDTARGET     = $(OWDPATH)/Release/OculusWorldDemo_$(SYSARCH)_$(RELEASETYPE)

####### Targets

all:    $(LIBOVRTARGET) $(OWDTARGET)

$(OWDTARGET): force_look $(LIBOVRTARGET)
	$(MAKE) -C $(OWDPATH)

$(LIBOVRTARGET): force_look
	$(MAKE) -C $(LIBOVRPATH)

run: $(OWDTARGET)
	$(MAKE) -C $(OWDPATH) run

clean:
	$(MAKE) -C $(LIBOVRPATH) clean
	$(MAKE) -C $(OWDPATH) clean

force_look:
	true

# Generate help based on descriptions of targets given in this Makefile.
help: 		##- Show this help
	@echo "Targets:"
	@echo "  all      : Build LibOVR and Oculus World Demo"
	@echo "  run      : Run Oculus World Demo"
	@echo "  clean    : Clean selected release (DEBUG=[0,1])"
	@echo "  cleanall : Clean all possible release targets"
	@echo ""
	@echo "Options:"
	@echo "  DEBUG    : 'make DEBUG=1' will build the current target in DEBUG mode"

# Experimental method of automatically generating help from target names.
#@grep -h "##-" $(MAKEFILE_LIST) | grep -v grep | sed -e 's/^/ /' | sed -e 's/\:\s*##-/:/' | awk '{printf "%+6s\n", $$0}'
