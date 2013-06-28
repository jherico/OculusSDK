/************************************************************************************

Filename    :   OVR_Posix_DeviceStatus.cpp
Content     :   Posix implementation of DeviceStatus.
Created     :   January 24, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#include "OVR_Posix_DeviceStatus.h"

#include "OVR_Posix_HIDDevice.h"

#include "Kernel/OVR_Log.h"

namespace OVR { namespace Posix {

DeviceStatus::DeviceStatus(Notifier* const pClient)
	: pNotificationClient(pClient)
{
}

bool DeviceStatus::Initialize()
{
	return true;
}

void DeviceStatus::ShutDown()
{
}

DeviceStatus::~DeviceStatus()
{
	OVR_ASSERT_LOG(hMessageWindow == NULL, ("Need to call 'ShutDown' from DeviceManagerThread."));
}

void DeviceStatus::ProcessMessages()
{
}

}} // namespace OVR::Posix
