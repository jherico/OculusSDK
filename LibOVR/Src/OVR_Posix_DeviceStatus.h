/************************************************************************************

Filename    :   OVR_Posix_DeviceStatus.h
Content     :   Posix-specific DeviceStatus header.
Created     :   January 24, 2013
Authors     :   Lee Cooper

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#ifndef OVR_Posix_DeviceStatus_h
#define OVR_Posix_DeviceStatus_h

#include "Kernel/OVR_String.h"
#include "Kernel/OVR_RefCount.h"
#include "Kernel/OVR_Array.h"

namespace OVR { namespace Posix {

class DeviceStatus : public RefCountBase<DeviceStatus>
{
public:

	// Notifier used for device messages.
	class Notifier
	{
	public:
		enum MessageType
		{
			DeviceAdded     = 0,
			DeviceRemoved   = 1,
		};

		virtual bool OnMessage(MessageType type, const String& devicePath)
        { OVR_UNUSED2(type, devicePath); return true; }
	};

	DeviceStatus(Notifier* const pClient);
	virtual ~DeviceStatus();

	void operator = (const DeviceStatus&);	// No assignment implementation.

	bool Initialize();
	void ShutDown();

	void ProcessMessages();

private: // data
    Notifier* const     pNotificationClient;	// Don't reference count a back-pointer.
};

}} // namespace OVR::Posix

#endif // OVR_Posix_DeviceStatus_h
