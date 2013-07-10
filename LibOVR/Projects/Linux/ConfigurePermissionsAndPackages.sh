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
#############################################################################

if [ "$(id -u)" != "0" ]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

echo "Installing OculusVR Rift udev rules file..."
cp 90-oculus.rules /lib/udev/rules.d
echo "Installing tinyxml2..."
libtinyxml2-dev
echo "Installing libudev..."
apt-get install libudev-dev
echo "Installing libext..."
apt-get install libext-dev
echo "Installing freeglut3..."
apt-get install freeglut3-dev
echo "Installing Xinerama..."
apt-get install libxinerama-dev
echo "Installation complete"

