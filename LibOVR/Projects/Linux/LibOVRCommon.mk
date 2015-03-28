#############################################################################
#
# Filename    : LibOVRCommon.mk
# Content     : Shared makefile.
# Created     : November 2014
# Copyright   : Copyright 2014 OculusVR, Inc. All Rights Reserved
#
#############################################################################

# Must occur before other includes in this file. MAKEFILE_LIST contains a
# list of include files.
THIS_MAKE_DIR:= $(dir $(lastword $(MAKEFILE_LIST)))

include $(THIS_MAKE_DIR)LibOVRConfig.mk

####### Set SYSSIZE (32 or 64).
SYSSIZE = 32
ifneq (,$(findstring x86_64,$(SYSARCH)))
SYSSIZE = 64
endif
export SYSSIZE

####### Set internal definitions if not already set.
PREPROCDEFS?= unassigned
ifeq ($(PREPROCDEFS),unassigned)

ifeq ($(INCLUDE_PRIVATE_DEFS),1)
PREPROCDEFS= -DOVR_PRIVATE_FILE=''
else
PREPROCDEFS=
endif

SINGLEPROCESS ?= 0
STATIC        ?= 0
RELEASESUFFIX=
ifeq ($(SINGLEPROCESS),1)
	PREPROCDEFS    += -DOVR_SINGLE_PROCESS
	RELEASESUFFIX  := SingleProcess
endif
ifeq ($(STATIC),1)
	PREPROCDEFS    += -DOVR_STATIC_BUILD
	RELEASESUFFIX  := $(RELEASESUFFIX1)Static
else
	PREPROCDEFS    += -DOVR_DLL_BUILD
	RELEASESUFFIX  := $(RELEASESUFFIX1)
endif
export RELEASESUFFIX

# RELEASETYPE
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	RELEASETYPE = Debug$(RELEASESUFFIX)
else
	RELEASETYPE = Release$(RELEASESUFFIX)
endif
export RELEASETYPE

endif # ifeq ($(PREPROCDEFS),unassigned)
export PREPROCDEFS
