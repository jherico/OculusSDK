OVR_PRODUCT_VERSION ?= 0
OVR_MAJOR_VERSION   ?= 5
OVR_MINOR_VERSION   ?= 0
OVR_PATCH_VERSION   ?= 1
OVR_BUILD_VERSION   ?= 0

export OVR_PRODUCT_VERSION
export OVR_MAJOR_VERSION
export OVR_MINOR_VERSION
export OVR_PATCH_VERSION
export OVR_BUILD_VERSION

####### Compiler, tools and options
CXX           = g++
CC            = gcc
DELETEFILE    = rm -f
DELETEDIR     = rm -fr

####### Set the system architecture if not already set.
SYSARCH ?= unassigned
ifeq ($(SYSARCH),unassigned)
ifeq ($(shell uname -m),x86_64)
SYSARCH = x86_64
else
SYSARCH = i386
endif
endif
export SYSARCH
