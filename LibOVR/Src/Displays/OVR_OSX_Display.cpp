/************************************************************************************

Filename    :   OVR_OSX_Display.cpp
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

#include "OVR_OSX_Display.h"
#include "Kernel/OVR_Log.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>
#include <IOKit/graphics/IOGraphicsLib.h>

//-------------------------------------------------------------------------------------
// ***** Display enumeration Helpers

namespace OVR { 

// FIXME Code duplication with windows.
#define EDID_LENGTH                             0x80

#define EDID_HEADER                             0x00
#define EDID_HEADER_END                         0x07

#define ID_MANUFACTURER_NAME                    0x08
#define ID_MANUFACTURER_NAME_END                0x09

#define EDID_STRUCT_VERSION                     0x12
#define EDID_STRUCT_REVISION                    0x13

#define ESTABLISHED_TIMING_1                    0x23
#define ESTABLISHED_TIMING_2                    0x24
#define MANUFACTURERS_TIMINGS                   0x25

#define DETAILED_TIMING_DESCRIPTIONS_START      0x36
#define DETAILED_TIMING_DESCRIPTION_SIZE        18
#define NO_DETAILED_TIMING_DESCRIPTIONS         4

#define DETAILED_TIMING_DESCRIPTION_1           0x36
#define DETAILED_TIMING_DESCRIPTION_2           0x48
#define DETAILED_TIMING_DESCRIPTION_3           0x5a
#define DETAILED_TIMING_DESCRIPTION_4           0x6c

#define MONITOR_NAME            0xfc
#define MONITOR_LIMITS          0xfd
#define MONITOR_SERIAL			0xff

#define UNKNOWN_DESCRIPTOR      -1
#define DETAILED_TIMING_BLOCK   -2

#define DESCRIPTOR_DATA         5

const UByte edid_v1_header[] = { 0x00, 0xff, 0xff, 0xff,
	                            0xff, 0xff, 0xff, 0x00 };

const UByte edid_v1_descriptor_flag[] = { 0x00, 0x00 };

// FIXME Code duplication with windows. Refactor.
static int blockType( UByte* block )
{
	if ( !strncmp( (const char*)edid_v1_descriptor_flag, (const char*)block, 2 ) )
	{
		// descriptor
		if ( block[ 2 ] != 0 )
			return UNKNOWN_DESCRIPTOR;
		return block[ 3 ];
	}
    else
    {		
		return DETAILED_TIMING_BLOCK;
	}
}

static char* getMonitorName( UByte const* block )
{
	static char name[ 13 ];
	unsigned    i;
	UByte const* ptr = block + DESCRIPTOR_DATA;

	for( i = 0; i < 13; i++, ptr++ )
	{
		if ( *ptr == 0xa )
		{
			name[ i ] = 0;
			return name;
		}

		name[ i ] = *ptr;
	}

	return name;
}

// FIXME Code duplication with windows. Refactor.
static bool parseEdid( UByte* edid, OVR::OSX::DisplayEDID& edidResult )
{
    unsigned i;
    UByte* block;
    const char* monitor_name = "Unknown";
    UByte checksum = 0;

    for( i = 0; i < EDID_LENGTH; i++ )
        checksum += edid[ i ];

    // Bad checksum, fail EDID
    if (  checksum != 0  )
        return false;

    if ( strncmp( (const char*)edid+EDID_HEADER, (const char*)edid_v1_header, EDID_HEADER_END+1 ) )
    {
        // First bytes don't match EDID version 1 header
        return false;
    }


    // OVR_DEBUG_LOG_TEXT(( "\n# EDID version %d revision %d\n",
    //                     (int)edid[EDID_STRUCT_VERSION],(int)edid[EDID_STRUCT_REVISION] ));

    // Monitor name and timings 

    char serialNumber[14];
    memset( serialNumber, 0, 14 );

    block = edid + DETAILED_TIMING_DESCRIPTIONS_START;

    for( i = 0; i < NO_DETAILED_TIMING_DESCRIPTIONS; i++,
        block += DETAILED_TIMING_DESCRIPTION_SIZE )
    {

        if ( blockType( block ) == MONITOR_NAME )
        {
            monitor_name = getMonitorName( block );
        }

        if( blockType( block ) == MONITOR_SERIAL )
        {
            memcpy( serialNumber, block + 5, 13 );
            break;
        }
    }

    UByte vendorString[4] = {0};

    vendorString[0] = (edid[8] >> 2 & 31) + 64;
    vendorString[1] = ((edid[8] & 3) << 3) | (edid[9] >> 5) + 64;
    vendorString[2] = (edid[9] & 31) + 64;

    edidResult.ModelNumber  = *(UInt16*)&edid[10];
    edidResult.MonitorName  = OVR::String(monitor_name);
    edidResult.VendorName   = OVR::String((const char*)vendorString);
    edidResult.SerialNumber = OVR::String(serialNumber);
    
    // printf( "\tIdentifier \"%s\"\n", monitor_name );
    // printf( "\tVendorName \"%s\"\n", vendorString );
    // printf( "\tModelName \"%s\"\n", monitor_name );
    // printf( "\tModelNumber %d\n", edidResult.ModelNumber );
    // printf( "\tSerialNumber \"%s\"\n", edidResult.SerialNumber.ToCStr() );

    // FIXME: Get timings as well, though they aren't very useful here
    // except for the vertical refresh rate, presumably

    return true;
}

static int discoverExtendedRifts(OVR::OSX::DisplayDesc* descriptorArray, int inputArraySize, bool edidInfo)
{
    OVR_UNUSED(edidInfo);
    int result = 0;

    static bool reportDiscovery = true;
    OVR_UNUSED(reportDiscovery);

    CGDirectDisplayID Displays[32];
    uint32_t NDisplays = 0;
    CGGetOnlineDisplayList(32, Displays, &NDisplays);

    for (unsigned int i = 0; i < NDisplays; i++)
    {
        CGDirectDisplayID dispId = Displays[i];

        io_service_t port = CGDisplayIOServicePort(dispId);
        CFDictionaryRef DispInfo = IODisplayCreateInfoDictionary(port, kNilOptions);

        // Display[i]

        uint32_t vendor = CGDisplayVendorNumber(dispId);
        uint32_t product = CGDisplayModelNumber(dispId);

        CGRect desktop = CGDisplayBounds(dispId);
        Vector2i desktopOffset(desktop.origin.x, desktop.origin.y);
        
        if (vendor == 16082 && ( (product == 1)||(product == 2)||(product == 3) ) ) // 7" or HD
        {
            if( result >= inputArraySize )
            {
                CFRelease(DispInfo);
                return result;
            }

            int width  = static_cast<int>(CGDisplayPixelsWide(dispId));
            int height = static_cast<int>(CGDisplayPixelsHigh(dispId));
            Sizei monitorResolution(width, height);
            
            // Obtain and parse EDID data.
            CFDataRef data = 
                (CFDataRef)CFDictionaryGetValue(DispInfo, CFSTR(kIODisplayEDIDKey));
            if (!data)
            {
                CFRelease(DispInfo);
                OVR::LogError("[OSX Display] Unable to obtain EDID for Oculus product %d", product);
                continue;
            }
            UByte* edid = (UByte*)CFDataGetBytePtr(data);
            OSX::DisplayEDID edidResult;
            parseEdid( edid, edidResult );

            OVR::OSX::DisplayDesc& desc = descriptorArray[result++];
            desc.DisplayID                  = dispId;
            desc.ModelName                  = edidResult.MonitorName;   // User friendly string.
            desc.EdidSerialNumber           = edidResult.SerialNumber;
            desc.LogicalResolutionInPixels  = monitorResolution;
            desc.NativeResolutionInPixels   = monitorResolution;
            desc.DesktopDisplayOffset       = desktopOffset;
            desc.Rotation                   = 0;

            auto roughEqual = [](double a, double b) -> bool
            {
                return fabs(a - b) < 1.0;
            };

            switch (product)
            {
                    case 3: desc.DeviceTypeGuess = HmdType_DK2;       break;
                    case 2: desc.DeviceTypeGuess = HmdType_DKHDProto; break;
                    case 1: desc.DeviceTypeGuess = HmdType_DK1;       break;

                default:
                    case 0: desc.DeviceTypeGuess = HmdType_Unknown;   break;
            }

            bool portraitDevice = (desc.DeviceTypeGuess == HmdType_DK2);
            double rotation = fabs(CGDisplayRotation(dispId));
            if (roughEqual(rotation, 0))
            {
                desc.Rotation = portraitDevice ? 270 : 0;
            }
            else if (roughEqual(rotation, 90))
            {
                desc.Rotation = portraitDevice ? 0 : 90;
            }
            else if (roughEqual(rotation, 180))
            {
                desc.Rotation = portraitDevice ? 90 : 180;
            }
            else if (roughEqual(rotation, 270))
            {
                desc.Rotation = portraitDevice ? 180 : 270;
            }

            // Hard-coded defaults in case the device doesn't have the data itself.
            // DK2 prototypes (0003) or DK HD Prototypes (0002)                
            if (product == 3 || product == 2)
            {
                desc.LogicalResolutionInPixels  = Sizei(1920, 1080);
                desc.NativeResolutionInPixels   = Sizei(1080, 1920);
            }
            else
            {
                desc.LogicalResolutionInPixels  = monitorResolution;
                desc.NativeResolutionInPixels   = monitorResolution;
            }

            //OVR_DEBUG_LOG_TEXT(("Display Found %x:%x\n", vendor, product));
        }
        CFRelease(DispInfo);
    }

    return result;
}


//-------------------------------------------------------------------------------------
// ***** Display 

bool Display::Initialize()
{
    // Nothing to initialize. OS X only supports compatibility mode.
    return true;
}

void Display::Shutdown()
{
}


bool Display::GetDriverMode(bool& driverInstalled, bool& compatMode, bool& hideDK1Mode)
{
    driverInstalled = false;
    compatMode = true;
    hideDK1Mode = false;
    return true;
}

bool Display::SetDriverMode(bool /*compatMode*/, bool /*hideDK1Mode*/)
{
    return false;
}

DisplaySearchHandle* Display::GetDisplaySearchHandle()
{
	return new OSX::OSXDisplaySearchHandle();
}

bool Display::InCompatibilityMode( bool displaySearch )
{
	OVR_UNUSED( displaySearch );
    return true;
}

int Display::GetDisplayCount( DisplaySearchHandle* handle, bool extended, bool applicationOnly, bool edidInfo )
{
    OVR_UNUSED(applicationOnly);

	static int extendedCount = -1;

	OSX::OSXDisplaySearchHandle* localHandle = (OSX::OSXDisplaySearchHandle*)handle;
    if (localHandle == NULL)
    {
        OVR::LogError("[OSX Display] No search handle passed into GetDisplayCount. Return 0 rifts.");
        return 0;
    }

    if (extendedCount == -1 || extended)
    {
        extendedCount = discoverExtendedRifts(localHandle->cachedDescriptorArray, OSX::OSXDisplaySearchHandle::DescArraySize, edidInfo);
    }

	localHandle->extended = true;
	localHandle->extendedDisplayCount = extendedCount;
	int totalCount = extendedCount;

    /// FIXME: Implement application mode for OS X.
    localHandle->application = false;
    localHandle->applicationDisplayCount = 0;

    localHandle->displayCount = totalCount;

    return totalCount;
}

Ptr<Display> Display::GetDisplay( int index, DisplaySearchHandle* handle )
{
    Ptr<Display> result = NULL;

    if (index < 0)
    {
        OVR::LogError("[OSX Display] Invalid index given to GetDisplay.");
        return NULL;
    }

	OSX::OSXDisplaySearchHandle* localHandle = (OSX::OSXDisplaySearchHandle*)handle;
    if (localHandle == NULL)
    {
        OVR::LogError("[OSX Display] No search handle passed into GetDisplay. Return 0 rifts.");
        return NULL;
    }

    if (localHandle->extended)
    {
        if (index >= 0 && index < (int)localHandle->extendedDisplayCount)
        {
            return *new OSX::OSXDisplayGeneric(localHandle->cachedDescriptorArray[index]);
        }

        // index -= localHandle->extendedDisplayCount;
    }

    if (localHandle->application)
    {
        OVR::LogError("[OSX Display] Mac does not support application displays.");
    }

    return result;
}
} // namespace OVR
