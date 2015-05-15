/************************************************************************************

Filename    :   Service_Common_Compositor.cpp
Content     :   Common code shared between compositor client/server
Created     :   Sept 25, 2014
Authors     :   Chris Taylor

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

#include "Service_Common_Compositor.h"
#include "OVR_PadCheck.h"


namespace OVR { namespace Service {


//-----------------------------------------------------------------------------
// Tools

struct SerializedovrEyeRenderDesc
{
    ovrEyeType  Eye;
    ovrFovPort  Fov;
    ovrRecti	DistortedViewport; 	        /// Distortion viewport.
    ovrVector2f PixelsPerTanAngleAtCenter;  /// How many display pixels will fit in tan(angle) = 1.
    ovrVector3f HmdToEyeViewOffset;         /// Translation of each eye.
};
static_assert(sizeof(ovrEyeRenderDesc) == sizeof(SerializedovrEyeRenderDesc), "Need to update serialization.");

struct SerializedProfileRenderInfo
{
    String EyeCupType;
    float Eye2Nose[2];
    float Eye2Plate[2];
    int EyeReliefDial;
    bool  HSWDisabled;

};
static_assert(sizeof(ProfileRenderInfo) == sizeof(SerializedProfileRenderInfo), "Need to update serialization.");

static bool SerializeProfileRenderInfo(BitStream& bs, bool write, ProfileRenderInfo& prof)
{
    bs.Serialize(write, prof.EyeCupType);
    bs.Serialize(write, prof.Eye2Nose);
    bs.Serialize(write, prof.Eye2Plate);
    bs.Serialize(write, prof.EyeReliefDial);
    bs.Serialize(write, prof.HSWDisabled);
    return true;
}

static_assert(sizeof(Pose<double>) == sizeof(double) * 7, "Need to update serialization.");

static bool SerializeCenteringPose(BitStream& bs, bool write, Pose<double>& transform)
{
    bs.Serialize(write, transform.Rotation);
    return bs.Serialize(write, transform.Translation);
}


//-------------------------------------------------------------------------------------
// ***** OutputLatencyTimings

bool OutputLatencyTimings::Serialize(bool write, Net::BitStream& bs)
{
    bs.Serialize(write, LatencyRender);
    bs.Serialize(write, LatencyTimewarp);
    bs.Serialize(write, LatencyPostPresent);
    bs.Serialize(write, ErrorRender);
    return bs.Serialize(write, ErrorTimewarp);
}


//-----------------------------------------------------------------------------
// CompositorClientCreate_1

bool RPCCompositorClientCreateParams::Serialize(bool write, BitStream& bs)
{
    // did you forget to serialize something?
    OVR_ASSERT(sizeof (*this) == 200    // 64bit
            || sizeof (*this) == 192);  // 32bit

    bs.Serialize(write, ProcessId);
    bs.Serialize(write, HMD);
    bs.Serialize(write, AdapterID);
    bs.Serialize(write, FenceHandle);
    bs.Serialize(write, FrameQueueSemaphoreHandle);

    // Serialize RiftInfo
    bs.Serialize(write, RiftInfo.DisplayUUID);
    return SerializeProfileRenderInfo(bs, write, RiftInfo.OurProfileRenderInfo);
}

bool RPCCompositorClientCreateResult::Serialize(bool write, BitStream& bs)
{
#if defined(OVR_OS_WIN32)
    IPCKey.Serialize(write, bs);
#endif // OVR_OS_WIN32

    bs.Serialize(write, MaxNumLayers);
    return bs.Serialize(write, AppTimingName);
}


//-----------------------------------------------------------------------------
// CompositorTextureSetCreate_1

bool RPCCompositorTextureSetCreateParams::Serialize(bool write, BitStream& bs)
{
    bs.Serialize(write, HMD);

    // Serialize buffers
    uint32_t size = (write) ? (uint32_t)TextureShareHandles.GetSizeI() : 0;
    bs.Serialize(write, size);

    if (!write)
    {
        TextureShareHandles.Resize(size);
    }

    for (uint32_t i = 0; i < size; ++i)
    {
        bs.Serialize(write, TextureShareHandles[i]);
    }

    return true;
}

bool RPCCompositorTextureSetCreateResult::Serialize(bool write, BitStream& bs)
{
    return bs.Serialize(write, TextureSetID);
}


//-----------------------------------------------------------------------------
// CompositorTextureSetDestroy_1

bool RPCCompositorTextureSetDestroyParams::Serialize(bool write, BitStream& bs)
{
    bs.Serialize(write, HMD);
    bs.Serialize(write, TextureSetID);
    return true;
}

bool RPCCompositorTextureSetDestroyResult::Serialize(bool write, BitStream& bs)
{
    if (!bs.Serialize(write, ResultCode))
    {
        return false;
    }

    return bs.Serialize(write, DetailErrorCode);
}


//-----------------------------------------------------------------------------
// CompositorSubmitLayers : Sent over FastIPC or socket RPC

bool IPCCompositorSubmitLayersParams::Serialize(bool write, BitStream& bs)
{
    bs.Serialize(write, HMD);

    // Serialize layers
    uint32_t size = (write) ? (uint32_t)Layers.GetSizeI() : 0;
    bs.Serialize(write, size);

    if (!write)
    {
        Layers.Resize(size);
    }

    for (uint32_t layerNum = 0; layerNum < size; ++layerNum)
    {
        CompositorLayerDesc *layer = &(Layers[layerNum]);

        // Verify that serialization matches the structures.
        // The assert will print the new size if needed.
        bool failure = false;
#if (OVR_PTR_SIZE == 8) // 64-bit builds:
        OVR_SIZE_CHECK(layer->Desc, 208);
        OVR_SIZE_CHECK(*layer, 248);
#else
        OVR_SIZE_CHECK(layer->Desc, 192);
        OVR_SIZE_CHECK(*layer, 228);
#endif
        OVR_ASSERT(!failure);

        bs.Serialize(write, layer->LayerNum);
        bs.Serialize(write, layer->Desc.Type);
        bs.Serialize(write, layer->Desc.bAnisoFiltering);
        bs.Serialize(write, layer->Desc.Quality);
        bs.Serialize(write, layer->Desc.bTextureOriginAtBottomLeft);
        for (int eyeNum = 0; eyeNum < 2; eyeNum++)
        {
            bs.Serialize(write, layer->TextureSetIDColor[eyeNum]);
            bs.Serialize(write, layer->TextureSetIDDepth[eyeNum]);
            bs.Serialize(write, layer->TextureIndexColor[eyeNum]);
            bs.Serialize(write, layer->TextureIndexDepth[eyeNum]);
            bs.Serialize(write, layer->Desc.EyeTextureSize[eyeNum]);
            bs.Serialize(write, layer->Desc.EyeRenderViewport[eyeNum]);
            bs.Serialize(write, layer->Desc.EyeRenderFovPort[eyeNum]);
            bs.Serialize(write, layer->Desc.EyeRenderPose[eyeNum]);
            bs.Serialize(write, layer->Desc.QuadSize[eyeNum]);
        }
        bs.Serialize(write, layer->Desc.ProjectionDesc.Projection22);
        bs.Serialize(write, layer->Desc.ProjectionDesc.Projection23);
        bs.Serialize(write, layer->Desc.ProjectionDesc.Projection32);
    }

    return true;
}

bool IPCCompositorSubmitLayersResult::Serialize(bool write, BitStream& bs)
{
    OVR_UNUSED2(write, bs);
    return true;
}

//-----------------------------------------------------------------------------
// CompositorEndFrame : Sent over FastIPC or socket RPC

bool CompositorEndFrameAppTiming::Serialize(bool write, Net::BitStream& bs)
{
    bs.Serialize(write, AppFrameIndex);
    bs.Serialize(write, AppRenderIMUTime);
    return bs.Serialize(write, AppScanoutStartTime);
}

bool IPCCompositorEndFrameParams::Serialize(bool write, BitStream& bs)
{
    bs.Serialize(write, HMD);
    bs.Serialize(write, EnabledDistortionCaps);
    SerializeCenteringPose(bs, write, CenteredFromWorld);

    OVR_ASSERT(sizeof(ViewScaleDesc) == 7 * sizeof(float));
    bs.Serialize(write, ViewScaleDesc.HmdSpaceToWorldScaleInMeters);
    bs.Serialize(write, ViewScaleDesc.HmdToEyeViewOffset[0]);
    bs.Serialize(write, ViewScaleDesc.HmdToEyeViewOffset[1]);

    static_assert(OVR_ARRAY_COUNT(DistortionClearColor) == 4, "Update loop");
    for (int i = 0; i < 4; ++i)
    {
        bs.Serialize(write, DistortionClearColor[i]);
    }

    return AppTiming.Serialize(write, bs);
}

bool IPCCompositorEndFrameResult::Serialize(bool write, BitStream& bs)
{
    return LatencyTimings.Serialize(write, bs);
}


//-----------------------------------------------------------------------------
// CompositorCreateMirror_1

bool RPCCompositorClientCreateMirrorParams::Serialize(bool write, BitStream& bs)
{
    bs.Serialize(write, HMD);
    return bs.Serialize(write, TextureHandle);
}

bool RPCCompositorClientCreateMirrorResult::Serialize(bool write, BitStream& bs)
{
    OVR_UNUSED2(write, bs);
    return true;
}

//-----------------------------------------------------------------------------
// CompositorDestroyMirror_1

bool RPCCompositorClientDestroyMirrorParams::Serialize(bool write, BitStream& bs)
{
    return bs.Serialize(write, HMD);
}

bool RPCCompositorClientDestroyMirrorResult::Serialize(bool write, BitStream& bs)
{
    OVR_UNUSED2(write, bs);
    return true;
}

//-----------------------------------------------------------------------------
// CompositorDestroy_1

bool RPCCompositorClientDestroyParams::Serialize(bool write, BitStream& bs)
{
    return bs.Serialize(write, HMD);
}

bool RPCCompositorClientDestroyResult::Serialize(bool write, BitStream& bs)
{
    OVR_UNUSED2(write, bs);
    return true;
}


}} // namespace OVR::Service
