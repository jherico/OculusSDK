#!/bin/bash

#############################################################################
#
# Filename    : OculusWorldDemo.sh
# Content     : Linux file for starting the OculusWorldDemo app from the SDK
#               package root.  It will determine the architechture of the 
#               system it is running on, and start the approprite executable
# Created     : 2013
# Authors     : Simon Hallam
# Copyright   : Copyright 2013 OculusVR, Inc. All Rights Reserved
# Instruction : Ensure that the OculusWorldDemo.sh has execute permissions.
#               Navigate to a command shell, enter:
#
#               ./OculusWorldDemo.sh
#
#############################################################################

MACHINE_TYPE=`uname -m`
if [ ${MACHINE_TYPE} == 'x86_64' ]; then
  ./Samples/OculusWorldDemo/Release/OculusWorldDemo_x86_64_Release
else
  ./Samples/OculusWorldDemo/Release/OculusWorldDemo_i386_Release
fi

