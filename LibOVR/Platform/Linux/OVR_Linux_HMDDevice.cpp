/************************************************************************************

Filename    :   OVR_Linux_HMDDevice.h
Content     :   Linux HMDDevice implementation
Created     :   June 17, 2013
Authors     :   Brant Lewis

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR SDK License Version 2.0 (the "License");
you may not use the Oculus VR SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#include "OVR_Linux_HMDDevice.h"

#include "OVR_Linux_DeviceManager.h"

#include "OVR_Profile.h"

#include "edid.h"

namespace OVR { namespace Platform {

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
// ***** HMDDeviceFactory

HMDDeviceFactory HMDDeviceFactory::Instance;

void HMDDeviceFactory::EnumerateDevices(EnumerateVisitor& visitor)
{
    // For now we'll assume the Rift DK1 is attached in extended monitor mode. Ultimately we need to
    // use XFree86 to enumerate X11 screens in case the Rift is attached as a separate screen. We also
    // need to be able to read the EDID manufacturer product code to be able to differentiate between
    // Rift models.

    bool foundHMD = false;
    RRCrtc crtcId = 0;
    Display* display = XOpenDisplay(NULL);
    XRRScreenResources *screen = XRRGetScreenResources(display, DefaultRootWindow(display));
    for (int iscres = screen->noutput - 1; iscres >= 0; --iscres) {
        RROutput output = screen->outputs[iscres];
        MonitorInfo * mi = read_edid_data(display, output);
        if (mi == NULL) {
            continue;
        }

        XRROutputInfo * info = XRRGetOutputInfo (display, screen, output);
        if (0 == memcmp(mi->manufacturer_code, "OVR", 3)) {
            int x = -1, y = -1, w = -1, h = -1;
            if (info->connection == RR_Connected && info->crtc) {
                XRRCrtcInfo * crtc_info = XRRGetCrtcInfo (display, screen, info->crtc);
                x = crtc_info->x;
                y = crtc_info->y;
                w = crtc_info->width;
                h = crtc_info->height;
                XRRFreeCrtcInfo(crtc_info);
            }
            char buffer[512];
            sprintf(buffer, "%s%04d", mi->manufacturer_code, mi->product_code);
            HMDDeviceCreateDesc hmdCreateDesc(this, info->name, buffer);
            hmdCreateDesc.SetScreenParameters(x, y, w, h, 0.14976f, 0.0936f, 0.0936f*0.5f, 0.0635f);
            // Notify caller about detected device. This will call EnumerateAddDevice
            // if the this is the first time device was detected.
            visitor.Visit(hmdCreateDesc);
            foundHMD = true;
            break;
        } // if

        OVR_DEBUG_LOG_TEXT(("DeviceManager - HMD Found %s - %d\n",
                            mi->dsc_product_name, screen->outputs[iscres]));
        XRRFreeOutputInfo (info);
        delete mi;
    } // for
    XRRFreeScreenResources(screen);

    // Real HMD device is not found; however, we still may have a 'fake' HMD
    // device created via SensorDeviceImpl::EnumerateHMDFromSensorDisplayInfo.
    // Need to find it and set 'Enumerated' to true to avoid Removal notification.
    if (!foundHMD) {
        Ptr<DeviceCreateDesc> hmdDevDesc = getManager()->FindDevice("", Device_HMD);
        if (hmdDevDesc)
            hmdDevDesc->Enumerated = true;
    }
}

#include "OVR_Common_HMDDevice.cpp"

}} // namespace OVR::Linux


