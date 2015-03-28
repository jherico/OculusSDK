/************************************************************************************

PublicHeader:   None
Filename    :   OVR_Display.cpp
Content     :   Common implementation for display device
Created     :   May 6, 2014
Notes       : 

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

************************************************************************************/

#include "OVR_Display.h"


namespace OVR {


// Place platform-independent code here

static bool DirectDisplayInitialized = false;

bool Display::GetDirectDisplayInitialized()
{
    return DirectDisplayInitialized;
}

void Display::SetDirectDisplayInitialized(bool initialized)
{
    DirectDisplayInitialized = initialized;
}


//-----------------------------------------------------------------------------
// EDID Parsing

// FIXME: This can be done much more compactly without the bitfields.
#pragma pack(push, 1)

#if defined(_MSC_VER)
#pragma warning(disable: 4201)  // Nameless struct/union
#endif

// All of our EDIDs use the Detailed timing descriptors, and not the 
// older Standard timing info in the EDID. Conforming EDID v1.3+ displays
// always put their preferred resolution, refresh, and timing info into the
// first Detailed timing descriptor.

#ifndef byte_t
typedef unsigned char byte_t;
#endif

//static const uint32_t EDIDv13Size = 128;              // Standard EDID v1.3 is 128 bytes.
static const uint32_t FirstDetailedTimingOffset = 54; // Detailed timing table starts here.
static const uint32_t DetailedTimingDescriptorCount = 4;
static const byte_t   MonitorSerialNumberType = 0xFF;
static const byte_t   MonitorNameType = 0xFC;

// Expected signature of EDID
static const byte_t EDIDSignature[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };

struct EDID_Header
{
    byte_t Signature[8];

    byte_t VendorIDHigh;
    byte_t VendorIDLow;

    uint16_t ProductCode;
    uint32_t SerialNumber;

    // We don't currently use anything farther into the header
    byte_t Unused[112];
};

struct EDID_Detailed_Timing_Descriptor
{
    uint16_t PixelClock;        // In 10Khz units

    byte_t HActivePixelsLSB;
    byte_t HBlankingPixelsLSB;

    union
    {
        struct
        {
            byte_t HBlankingPixelsMSB : 4;
            byte_t HActivePixelsMSB : 4;
        } Values;
        byte_t Value_;
    } HSizeMSB;

    byte_t VActivePixelsLSB;
    byte_t VBlankingPixelsLSB;
    union
    {
        struct
        {
            byte_t VBlankingPixelsMSB : 4;
            byte_t VActivePixelsMSB : 4;
        } Values;
        byte_t Value_;
    } VSizeMSB;

    byte_t HSyncOffsetPixelsLSB;
    byte_t HSyncPulseWidthPixelsLSB;

    union
    {
        struct
        {
            byte_t VSyncPulseWidthLSB : 4;
            byte_t VSyncOffsetPixelsLSB : 4;
        } Values;
        byte_t Value_;
    } VSync;

    union
    {
        struct
        {
            byte_t VSyncPulseWidthMSB : 2;
            byte_t VSyncOffsetMSB : 2;
            byte_t HSyncPulseWidthMSB : 2;
            byte_t HSyncOffsetMSB : 2;
        } Values;
        byte_t Value_;
    } SyncMSB;

    byte_t Unused[6]; // We don't use anything else in the header
};

struct EDID_Other_Descriptor
{
    byte_t Reserved[3];
    byte_t Type;
    byte_t Reserved1;
    char Data[13];
};

#pragma pack(pop)


static void StripTrailingWhitespace(char* str)
{
    // Get initial string length
    int mlen = (int)strlen(str);

    // While removing trailing characters,
    while (mlen > 0)
    {
        // If trailing character should be stripped,
        char trailing = str[mlen - 1];
        if (trailing == '\n' || trailing == ' ')
        {
            // Strip it and reduce string length.
            str[mlen - 1] = '\0';
            --mlen;
        }
        else
        {
            // Stop here.
            break;
        }
    }
}

bool DisplayEDID::Parse(const unsigned char* edid)
{
    const EDID_Header* header = (const EDID_Header*)edid;

    if (memcmp(header->Signature, EDIDSignature, sizeof(EDIDSignature)) != 0)
    {
        OVR_ASSERT(false);
        return false;
    }

    memset(VendorName,   0, sizeof(VendorName));
    memset(MonitorName,  0, sizeof(MonitorName));
    memset(SerialNumber, 0, sizeof(SerialNumber));

    // Extract the 5-bit chars for the Vendor ID (PNP code)
    uint16_t char1 = (header->VendorIDHigh >> 2) & 0x1F;
    uint16_t char2 = ((header->VendorIDHigh & 0x2) << 3) | (header->VendorIDLow >> 5);
    uint16_t char3 = header->VendorIDLow & 0x1F;
    VendorName[0]  = (char)char1 - 1 + 'A';
    VendorName[1] = (char)char2 - 1 + 'A';
    VendorName[2] = (char)char3 - 1 + 'A';
    VendorName[3]  = '\0';
    ModelNumber    = header->ProductCode;

    const EDID_Detailed_Timing_Descriptor* detailedTiming = (const EDID_Detailed_Timing_Descriptor*)&edid[FirstDetailedTimingOffset];

    // First Detailed timing info is always preferred mode:
    uint32_t hActive   = (detailedTiming->HSizeMSB.Values.HActivePixelsMSB << 8) | detailedTiming->HActivePixelsLSB;
    uint32_t vActive   = (detailedTiming->VSizeMSB.Values.VActivePixelsMSB << 8) | detailedTiming->VActivePixelsLSB;
    uint32_t hBlanking = (detailedTiming->HSizeMSB.Values.HBlankingPixelsMSB << 8) | detailedTiming->HBlankingPixelsLSB;
    uint32_t vBlanking = (detailedTiming->VSizeMSB.Values.VBlankingPixelsMSB << 8) | detailedTiming->VBlankingPixelsLSB;

    // Need to scale up the values, since they're in 10Khz, and we're using integer math without fractions
    uint32_t denom = 1000;
    uint32_t totalPixels = (hActive + hBlanking) * (vActive + vBlanking);
    uint32_t vSyncNumerator = (uint32_t)(((uint64_t)detailedTiming->PixelClock * 10000 * denom) / totalPixels);

    Width  = hActive;
    Height = vActive;

    RefreshNumerator   = vSyncNumerator;
    RefreshDenominator = denom;

    // The remaining ones can hold extra info. Look for monitor name & serial number strings.
    ++detailedTiming;
    for (int i = 1; i < (int)DetailedTimingDescriptorCount; ++i, ++detailedTiming)
    {
        if (detailedTiming->PixelClock == 0)
        {
            // Not a timing info, use OtherDescriptor instead
            const EDID_Other_Descriptor* other = (const EDID_Other_Descriptor*)detailedTiming;
            switch (other->Type)
            {
            case MonitorNameType:
                static_assert(sizeof(MonitorName) == sizeof(other->Data) + 1, "serial number field size is off");
                memcpy(MonitorName, other->Data, sizeof(MonitorName));
                MonitorName[sizeof(MonitorName) - 1] = '\0';
                StripTrailingWhitespace(MonitorName);
                break;

            case MonitorSerialNumberType:
                static_assert(sizeof(SerialNumber) == sizeof(other->Data) + 1, "serial number field size is off");
                memcpy(SerialNumber, other->Data, sizeof(SerialNumber));
                SerialNumber[sizeof(SerialNumber) - 1] = '\0';
                StripTrailingWhitespace(SerialNumber);
                break;

            default:
                break;
            }
        }
    }

    return true;
}

HmdTypeEnum HmdTypeFromModelNumber(int modelNumber)
{
    HmdTypeEnum deviceTypeGuess = HmdType_Unknown;
    switch (modelNumber)
    {
    case 3: deviceTypeGuess = HmdType_DK2;       break;
    case 2: deviceTypeGuess = HmdType_DKHDProto; break;
    case 1: deviceTypeGuess = HmdType_DK1;       break;
    default: break;
    }
    return deviceTypeGuess;
}

bool Display::MatchDisplay(const Display* other)
{
    // Note this is not checking the DeviceName, which corresponds to which monitor the device is.
    // This allows matching to match a display that has changed how it is plugged in.
    // The rotation must match, which allows us to react properly by regenerating the HMD info.
    bool displayMatch =
        (DisplayID == other->DisplayID) &&
        (EdidSerialNumber == other->EdidSerialNumber) &&
        (NativeResolutionInPixels == other->NativeResolutionInPixels) &&
        (Rotation == other->Rotation) &&
        (ApplicationExclusive == other->ApplicationExclusive);

    return displayMatch;
}


} // namespace OVR
