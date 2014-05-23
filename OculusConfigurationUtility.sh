#!/bin/bash

#############################################################################
#
# Filename    : OculusConfigurationUtility.sh
# Content     : Linux file for starting the OculusConfigurationUtility app 
#               from the SDK package root.  It will determine the 
#               architechture of the system it is running on, and start the 
#               approprite executable
# Created     : 2013
# Authors     : Simon Hallam
# Copyright   : Copyright 2013 OculusVR, Inc. All Rights Reserved
# Instruction : Ensure that the OculusConfigurationUtility.sh has execute 
#               permissions.  Navigate to a command shell, enter:
#
#               ./OculusConfigurationUtility.sh
#
#############################################################################

MACHINE_TYPE=`uname -m`
if [ ${MACHINE_TYPE} == 'x86_64' ]; then
  ./Tools/OculusConfigUtil/OculusConfigUtil_x86_64
elif [ ${MACHINE_TYPE} == 'i686' ]; then
  ./Tools/OculusConfigUtil/OculusConfigUtil_i386
else
  echo "The Oculus Configuration Utility does not currently support this platform."
fi

