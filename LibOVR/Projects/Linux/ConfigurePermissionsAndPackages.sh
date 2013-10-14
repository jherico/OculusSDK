#!/bin/bash

#############################################################################
#
# Filename    : ConfigurePermissionsAndPackages.sh
# Content     : Linux file for installing prerequisite libraries and the 
#               permissions file for the USB HID device
# Created     : 2013
# Authors     : Simon Hallam and Brant Lewis
# Copyright   : Copyright 2013 OculusVR, Inc. All Rights Reserved
# Instruction : Ensure that the install.sh has execute permissions.
#               Navigate to a command shell, enter:
#               
#               sudo ./install.sh
#
#		Enter the administrator password for access.
#
# Notes       : UBUNTU 13 USERS
#               ---------------
#                 The OculusConfigUtil does not currently support Ubuntu 13
#                 out of the box.  If you see an error similar to this upon
#                 launching OculusConfigUtil:
#
#                     "error while loading shared libraries: libudev.so.0"
#
#                 Then please try the following solution, until we officially 
#                 support Ubuntu 13:
#
#                     cd /lib/x86_64-linux-gnu/
#                     sudo ln -sf libudev.so.1 libudev.so.0
#
#############################################################################

if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

echo "Installing OculusVR Rift udev rules file..."
DIR=$(dirname "$0")
cp "$DIR"/90-oculus.rules /lib/udev/rules.d
echo "Installing tinyxml2..."
apt-get install libtinyxml2-dev
echo "Installing libudev..."
apt-get install libudev-dev
echo "Installing libxext..."
apt-get install libxext-dev
echo "Installing mesa-common..."
apt-get install mesa-common-dev
echo "Installing freeglut3..."
apt-get install freeglut3-dev
echo "Installing Xrandr..."
apt-get install libxrandr-dev
echo "Installation complete"

