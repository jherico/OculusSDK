/************************************************************************************

Filename    :   OVR_OSX_Display.h
Content     :   OSX-specific Display declarations
Created     :   July 2, 2014
Authors     :   James Hughes

Copyright   :   Copyright 2014 Oculus VR, LLC All Rights reserved.

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

#ifndef OVR_OSX_Display_h
#define OVR_OSX_Display_h

#include "OVR_Display.h"

namespace OVR { namespace OSX {


//-------------------------------------------------------------------------------------
// DisplayDesc

// Display information enumerable through OS .
// TBD: Should we just move this to public header, so it's a const member of Display?
struct DisplayDesc
{
    DisplayDesc() :
        DeviceTypeGuess(HmdType_None),
        DisplayID(0),
        LogicalResolutionInPixels(0),
        NativeResolutionInPixels(0)
    {}

    HmdTypeEnum DeviceTypeGuess;
    uint32_t    DisplayID; // This is the device identifier string from MONITORINFO (for app usage)
    String      ModelName; // This is a "DK2" type string
    String      EdidSerialNumber;
    Sizei       LogicalResolutionInPixels;
    Sizei       NativeResolutionInPixels;
    Vector2i    DesktopDisplayOffset;
    int         Rotation;
};


//-------------------------------------------------------------------------------------
// DisplayEDID

// Describes EDID information as reported from our display driver.
struct DisplayEDID
{
    DisplayEDID() :
        ModelNumber(0)
    {}

    String MonitorName;
    UInt16 ModelNumber;
    String VendorName;
    String SerialNumber;
};

//-------------------------------------------------------------------------------------
// OSX Display Search Handle
class OSXDisplaySearchHandle : public DisplaySearchHandle
{
public:
    OSXDisplaySearchHandle() :
        extended(false),
        application(false),
        extendedDisplayCount(0),
        applicationDisplayCount(0),
        displayCount(0)
    {}
    virtual ~OSXDisplaySearchHandle()   {}

    static const int DescArraySize = 16;

    OSX::DisplayDesc    cachedDescriptorArray[DescArraySize];
    bool                extended;
    bool                application;
    int                 extendedDisplayCount;
    int                 applicationDisplayCount;
    int                 displayCount;
};

//-------------------------------------------------------------------------------------
// OSXDisplayGeneric

// Describes OSX display in Compatibility mode, containing basic data
class OSXDisplayGeneric : public Display
{
public:
    OSXDisplayGeneric( const DisplayDesc& dd ) :
        Display(dd.DeviceTypeGuess,
                dd.DisplayID,
                dd.ModelName,
                dd.EdidSerialNumber,
                dd.LogicalResolutionInPixels,
                dd.NativeResolutionInPixels,
                dd.DesktopDisplayOffset,
                0,
                dd.Rotation,
				        false)
    {
    }

    virtual ~OSXDisplayGeneric()
    {
    }

    virtual bool InCompatibilityMode() const
    {
        return true;
    }

    // Generic displays are not capable of mirroring
    virtual MirrorMode SetMirrorMode( MirrorMode newMode ) 
    { 
        OVR_UNUSED( newMode ); 
        return MirrorDisabled; 
    } 
};

}} // namespace OVR::OSX

#endif // OVR_OSX_Display_h
