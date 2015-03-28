/************************************************************************************

Filename    :   OVR_Linux_UDEV.h
Content     :   This is the interface for libudev1 or libudev0 in Linux.

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.2 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.2

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#ifndef OVR_Linux_UDEV_h
#define OVR_Linux_UDEV_h

#include <dlfcn.h>
#include <assert.h>

namespace OVR
{
namespace Linux
{

bool LoadUDEVSymbols();

// Function pointers returned from dlsym (global, NOT in OVR::).
extern struct udev* (*udev_new)(void);
extern void (*udev_unref)(struct udev*);

extern struct udev_device* (*udev_device_new_from_syspath)(struct udev*, const char*);
extern const char* (*udev_device_get_action)(struct udev_device*);
extern const char* (*udev_device_get_devnode)(struct udev_device*);
extern struct udev_device* (*udev_device_get_parent_with_subsystem_devtype)(struct udev_device*, const char*, const char*);
extern const char* (*udev_device_get_sysattr_value)(struct udev_device*, const char*);
extern void (*udev_device_unref)(struct udev_device*);

extern struct udev_enumerate* (*udev_enumerate_new)(struct udev*);
extern int (*udev_enumerate_add_match_subsystem)(struct udev_enumerate*, const char*);
extern struct udev_list_entry* (*udev_enumerate_get_list_entry)(struct udev_enumerate*);
extern int (*udev_enumerate_scan_devices)(struct udev_enumerate*);
extern void (*udev_enumerate_unref)(struct udev_enumerate*);

extern const char* (*udev_list_entry_get_name)(struct udev_list_entry*);
extern struct udev_list_entry* (*udev_list_entry_get_next)(struct udev_list_entry *);

extern struct udev_monitor* (*udev_monitor_new_from_netlink)(struct udev*, const char*);
extern int (*udev_monitor_enable_receiving)(struct udev_monitor*);
extern int (*udev_monitor_filter_add_match_subsystem_devtype)(struct udev_monitor*, const char*, const char*);
extern int (*udev_monitor_get_fd)(struct udev_monitor*);
extern struct udev_device* (*udev_monitor_receive_device)(struct udev_monitor*);
extern void (*udev_monitor_unref)(struct udev_monitor*);

#define udev_list_entry_foreach(list_entry, first_entry) \
        for (list_entry = first_entry; \
             list_entry != NULL; \
             list_entry = udev_list_entry_get_next(list_entry))

}} // namespace OVR::Linux

#endif // OVR_Linux_UDEV_h
