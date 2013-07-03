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
#               ./install.sh
#
#		Enter the administrator password for sudo access.
#
#############################################################################

echo "Installing OculusVR Rift udev rules file..."
sudo cp ./LibOVR/90-oculus.rules /lib/udev/rules.d
echo "Installing libudev..."
sudo apt-get install libudev-dev
echo "Installing libext..."
sudo apt-get install libext-dev
echo "Installing mesa-common..."
sudo apt-get install mesa-common-dev
echo "Installing freeglut3..."
sudo apt-get install freeglut3-dev
echo "Installing Xinerama..."
sudo apt-get install libxinerama-dev
echo "Installation complete"

