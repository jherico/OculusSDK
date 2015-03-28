/************************************************************************************

Filename    :   Service_NetSessionCommon.cpp
Content     :   Server for service interface
Created     :   June 12, 2014
Authors     :   Kevin Jenkins, Chris Taylor

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

#include "Service_NetSessionCommon.h"
#include "../OVR_Stereo.h"

namespace OVR { namespace Service {


//-----------------------------------------------------------------------------
// NetSessionCommon

NetSessionCommon::NetSessionCommon() :
    Terminated(false)
{
    pSession = new Net::Session;
    OVR_ASSERT(pSession != NULL);

    pRPC = new Net::Plugins::RPC1;
    OVR_ASSERT(pRPC != NULL);

    pSession->AddSessionListener(pRPC);
}

NetSessionCommon::~NetSessionCommon()
{
    if (pSession)
    {
        delete pSession;
        pSession = NULL;
    }
    if (pRPC)
    {
        delete pRPC;
        pRPC = NULL;
    }

    Terminated.store(true, std::memory_order_relaxed);

    OVR_ASSERT(IsFinished());
}

void NetSessionCommon::onSystemDestroy()
{
    Terminated.store(true, std::memory_order_relaxed);

    Join();

    Release();
}

void NetSessionCommon::onThreadDestroy()
{
    Terminated.store(true, std::memory_order_relaxed);
    if (pSession)
    {
        pSession->Shutdown();
    }
}

template<typename T>
static bool SerializeUInt32(bool write, Net::BitStream* bitStream, T& data)
{
    int32_t w = 0;
    bool result = false;

    if (write)
    {
        w = (int32_t)data;
        result = bitStream->Serialize(write, w);
    }
    else
    {
        result = bitStream->Serialize(write, w);
        data = (T)w;
    }

    return result;
}

static bool SerializeBool(bool write, Net::BitStream* bitStream, bool& data)
{
    uint8_t x = 0;
    bool result = false;

    if (write)
    {
        x = data ? 1 : 0;
        result = bitStream->Serialize(write, x);
    }
    else
    {
        result = bitStream->Serialize(write, x);
        data = (x != 0);
    }

    return result;
}

bool NetSessionCommon::SerializeHMDInfo(Net::BitStream *bitStream, HMDInfo* hmdInfo, bool write)
{
    bool result = false;

    bitStream->Serialize(write, hmdInfo->ProductName);
    bitStream->Serialize(write, hmdInfo->Manufacturer);

    SerializeUInt32(write, bitStream, hmdInfo->Version);
    SerializeUInt32(write, bitStream, hmdInfo->HmdType);
    SerializeUInt32(write, bitStream, hmdInfo->ResolutionInPixels.w);
    SerializeUInt32(write, bitStream, hmdInfo->ResolutionInPixels.h);
    SerializeUInt32(write, bitStream, hmdInfo->ShimInfo.DeviceNumber);
    SerializeUInt32(write, bitStream, hmdInfo->ShimInfo.NativeWidth);
    SerializeUInt32(write, bitStream, hmdInfo->ShimInfo.NativeHeight);
    SerializeUInt32(write, bitStream, hmdInfo->ShimInfo.Rotation);

    bitStream->Serialize(write, hmdInfo->ScreenSizeInMeters.w);
    bitStream->Serialize(write, hmdInfo->ScreenSizeInMeters.h);
    bitStream->Serialize(write, hmdInfo->ScreenGapSizeInMeters);
    bitStream->Serialize(write, hmdInfo->CenterFromTopInMeters);
    bitStream->Serialize(write, hmdInfo->LensSeparationInMeters);

    SerializeUInt32(write, bitStream, hmdInfo->DesktopX);
    SerializeUInt32(write, bitStream, hmdInfo->DesktopY);
    SerializeUInt32(write, bitStream, hmdInfo->Shutter.Type);

    bitStream->Serialize(write, hmdInfo->Shutter.VsyncToNextVsync);
    bitStream->Serialize(write, hmdInfo->Shutter.VsyncToFirstScanline);
    bitStream->Serialize(write, hmdInfo->Shutter.FirstScanlineToLastScanline);
    bitStream->Serialize(write, hmdInfo->Shutter.PixelSettleTime);
    bitStream->Serialize(write, hmdInfo->Shutter.PixelPersistence);
    bitStream->Serialize(write, hmdInfo->DisplayDeviceName);

    SerializeUInt32(write, bitStream, hmdInfo->DisplayId);

    bitStream->Serialize(write, hmdInfo->PrintedSerial);

    SerializeBool(write, bitStream, hmdInfo->InCompatibilityMode);

    SerializeUInt32(write, bitStream, hmdInfo->VendorId);
    SerializeUInt32(write, bitStream, hmdInfo->ProductId);

    bitStream->Serialize(write, hmdInfo->CameraFrustumFarZInMeters);
    bitStream->Serialize(write, hmdInfo->CameraFrustumHFovInRadians);
    bitStream->Serialize(write, hmdInfo->CameraFrustumNearZInMeters);
    bitStream->Serialize(write, hmdInfo->CameraFrustumVFovInRadians);

    SerializeUInt32(write, bitStream, hmdInfo->FirmwareMajor);
    SerializeUInt32(write, bitStream, hmdInfo->FirmwareMinor);

    bitStream->Serialize(write, hmdInfo->PelOffsetR.x);
    bitStream->Serialize(write, hmdInfo->PelOffsetR.y);
    bitStream->Serialize(write, hmdInfo->PelOffsetB.x);
    result = bitStream->Serialize(write, hmdInfo->PelOffsetB.y);

    // Important please read before modifying!
    // ----------------------------------------------------
    // Please add new serialized data to the end, here.
    // Otherwise we will break backwards compatibility
    // and e.g. 0.4.4 runtime will not work with 0.4.3 SDK.

    // Note that whenever new fields are added here you
    // should also update the minor version of the RPC
    // protocol in OVR_Session.h so that clients fail at
    // a version check instead of when this data is
    // found to be truncated from the server.

    // The result of the final serialize should be returned to the caller.
    return result;
}

// Prefix key names with this to pass through to server
static const char* BypassPrefix = "server:";

static const char* KeyNames[][NetSessionCommon::ENumTypes] = {
    /* EGetStringValue */  { "CameraSerial", "CameraUUID", 0 },
    /* EGetBoolValue */    { "ReleaseDK2Sensors", "ReleaseLegacySensors", 0 },
    /* EGetIntValue */     { 0 },
    /* EGetNumberValue */  { "CenterPupilDepth", "LoggingMask", 0 },
    /* EGetNumberValues */ { "NeckModelVector3f", 0 },
    /* ESetStringValue */  { 0 },
    /* ESetBoolValue */    { "ReleaseDK2Sensors", "ReleaseLegacySensors", 0 },
    /* ESetIntValue */     { 0 },
    /* ESetNumberValue */  { "CenterPupilDepth", "LoggingMask", 0 },
    /* ESetNumberValues */ { "NeckModelVector3f", 0 },
};

bool IsInStringArray(const char* a[], const char* key)
{
    for (int i = 0; a[i]; ++i)
    {
        if (OVR_strcmp(a[i], key) == 0)
            return true;
    }

    return false;
}

const char *NetSessionCommon::FilterKeyPrefix(const char* key)
{
    // If key starts with BypassPrefix,
    if (strstr(key, BypassPrefix) == key)
    {
        key += strlen(BypassPrefix);
    }

    return key;
}

bool NetSessionCommon::IsServiceProperty(EGetterSetters e, const char* key)
{
    if ((e >= 0 && e < ENumTypes) && IsInStringArray(KeyNames[e], key))
    {
        return true;
    }

    // If key starts with BypassPrefix,
    if (strstr(key, BypassPrefix) == key)
    {
        return true;
    }

    return false;
}


}} // namespace OVR::Service
