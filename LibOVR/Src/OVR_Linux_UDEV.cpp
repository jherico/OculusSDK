/************************************************************************************

Filename    :   OVR_Linux_UDEV.cpp
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

#include "OVR_Linux_UDEV.h"

namespace OVR
{
namespace Linux
{

static void *udev_library = nullptr;

#define LOAD_UDEV_SYMBOL(symbol) \
    dlerror(); \
    ((void*&)(symbol)) = dlsym(udev_library, #symbol); \
    if (dlerror()) return false;

bool LoadUDEVSymbols()
{
    if (udev_library)
    {
        return true;
    }

    udev_library = nullptr;
    udev_library = dlopen("libudev.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if (!udev_library)
    {
        udev_library = dlopen("libudev.so.0", RTLD_LAZY | RTLD_GLOBAL);
    }
    assert(udev_library);

    LOAD_UDEV_SYMBOL(udev_new);
    LOAD_UDEV_SYMBOL(udev_unref);

    LOAD_UDEV_SYMBOL(udev_device_new_from_syspath);
    LOAD_UDEV_SYMBOL(udev_device_get_action);
    LOAD_UDEV_SYMBOL(udev_device_get_devnode);
    LOAD_UDEV_SYMBOL(udev_device_get_parent_with_subsystem_devtype);
    LOAD_UDEV_SYMBOL(udev_device_get_sysattr_value);
    LOAD_UDEV_SYMBOL(udev_device_unref);

    LOAD_UDEV_SYMBOL(udev_enumerate_new);
    LOAD_UDEV_SYMBOL(udev_enumerate_add_match_subsystem);
    LOAD_UDEV_SYMBOL(udev_enumerate_get_list_entry);
    LOAD_UDEV_SYMBOL(udev_enumerate_scan_devices);
    LOAD_UDEV_SYMBOL(udev_enumerate_unref);

    LOAD_UDEV_SYMBOL(udev_list_entry_get_name);
    LOAD_UDEV_SYMBOL(udev_list_entry_get_next);

    LOAD_UDEV_SYMBOL(udev_monitor_new_from_netlink);
    LOAD_UDEV_SYMBOL(udev_monitor_enable_receiving);
    LOAD_UDEV_SYMBOL(udev_monitor_filter_add_match_subsystem_devtype);
    LOAD_UDEV_SYMBOL(udev_monitor_get_fd);
    LOAD_UDEV_SYMBOL(udev_monitor_receive_device);
    LOAD_UDEV_SYMBOL(udev_monitor_unref);

    return true;
}

#undef LOAD_UDEV_SYMBOL

// Function pointers returned from dlsym (global, NOT in OVR::).
struct udev* (*udev_new)(void);
void (*udev_unref)(struct udev*);

struct udev_device* (*udev_device_new_from_syspath)(struct udev*, const char*);
const char* (*udev_device_get_action)(struct udev_device*);
const char* (*udev_device_get_devnode)(struct udev_device*);
struct udev_device* (*udev_device_get_parent_with_subsystem_devtype)(struct udev_device*, const char*, const char*);
const char* (*udev_device_get_sysattr_value)(struct udev_device*, const char*);
void (*udev_device_unref)(struct udev_device*);

struct udev_enumerate* (*udev_enumerate_new)(struct udev*);
int (*udev_enumerate_add_match_subsystem)(struct udev_enumerate*, const char*);
struct udev_list_entry* (*udev_enumerate_get_list_entry)(struct udev_enumerate*);
int (*udev_enumerate_scan_devices)(struct udev_enumerate*);
void (*udev_enumerate_unref)(struct udev_enumerate*);

const char* (*udev_list_entry_get_name)(struct udev_list_entry*);
struct udev_list_entry* (*udev_list_entry_get_next)(struct udev_list_entry *);

struct udev_monitor* (*udev_monitor_new_from_netlink)(struct udev*, const char*);
int (*udev_monitor_enable_receiving)(struct udev_monitor*);
int (*udev_monitor_filter_add_match_subsystem_devtype)(struct udev_monitor*, const char*, const char*);
int (*udev_monitor_get_fd)(struct udev_monitor*);
struct udev_device* (*udev_monitor_receive_device)(struct udev_monitor*);
void (*udev_monitor_unref)(struct udev_monitor*);

}} // namespace OVR::Linux
