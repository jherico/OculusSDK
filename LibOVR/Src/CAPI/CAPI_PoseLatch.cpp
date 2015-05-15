/************************************************************************************

Filename    :   CAPI_PoseLatch.cpp
Content     :   Late-latching pose matrices
Created     :   Dec 5, 2014
Authors     :   Chris Taylor, James Hughes

Copyright   :   Copyright 2015 Oculus VR, LLC All Rights reserved.

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

#include "CAPI_PoseLatch.h"

#include <Kernel/OVR_Log.h>
#include <Kernel/OVR_Timer.h>
#include <Kernel/OVR_Win32_IncludeWindows.h>
#include "CAPI_HMDState.h"
#include <Tracing/Tracing.h>
#include "D3D1X\CAPI_D3D11_DistortionRenderer.h"

//// ETW

#ifdef OVR_LATCH_WRITE_ETW

void OVR::CAPI::PoseLatch::ETW_WriteUpdate(const DebugStruct& debug)
{
    // FIXME: Add quaternion and position.
    TracePoseLatchCPUWrite(
        (uint32_t)GetCurrentThreadId(), 
        debug.Sequence,
        debug.Layer,
        debug.MotionSensorTime, 
        debug.PredictedScanlineFirst,
        0.0,  // TODO: Add predicted scanline last.
        OVR::Timer::GetSeconds());
}

void OVR::CAPI::PoseLatch::ETW_WriteLatchResult(const DebugStruct& debug)
{
    TracePoseLatchGPULatchReadback(
        (uint32_t)GetCurrentThreadId(),
        debug.Sequence,
        debug.Layer,
        debug.MotionSensorTime,
        debug.PredictedScanlineFirst,
        0.0,
        OVR::Timer::GetSeconds());
}

#endif


namespace OVR { namespace CAPI {


PoseLatch::PoseLatch() :
    NextActiveIndex(0),
    UpdateSequence(0),
    CurrentLayer(0),
    PinnedMemory(nullptr),
    HavePriorStagingData(false)
{
}

PoseLatch::~PoseLatch()
{
}

OVRError PoseLatch::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int layer)
{
    HRESULT hr;

    /// TODO: Clean up side-effects of this function. Task: 6768964

    OVR_ASSERT(sizeof(RingStruct) == 4 * 4 + RingElementCount * (16 * 4 * 4 + 8 * 2));

    CurrentLayer = layer;

    // BindFlags can be D3D11_BIND_VERTEX_BUFFER: "Map cannot be called
    // with MAP_WRITE_NO_OVERWRITE access because it can only be used by
    // D3D11_USAGE_DYNAMIC resources which were created with GPU Input BindFlags
    // were restricted to only D3D11_BIND_VERTEX_BUFFER and D3D11_BIND_INDEX_BUFFER."
    // D3D11_BIND_CONSTANT_BUFFER can also support D3D11_MAP_WRITE_NO_OVERWRITE
    // access when appropriately supported by the driver:
    // D3D11_FEATURE_DATA_D3D11_OPTIONS.MapNoOverwriteOnDynamicConstantBuffer = TRUE.
    D3D11_BUFFER_DESC mappedBufferDesc = {};
    mappedBufferDesc.ByteWidth      = sizeof(RingStruct);
    mappedBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
    mappedBufferDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    mappedBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    MappedBuffer = nullptr;
    hr = device->CreateBuffer(&mappedBufferDesc, nullptr, &MappedBuffer.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "CreateBuffer mapped");
    OVR_D3D_TAG_OBJECT(MappedBuffer);

    // Obtain pinned memory pointer. We have had discussions with both
    // Nvidia and AMD regarding using this memory region after unmapping the
    // buffer. The memory stays pinned and we can write to the memory
    // location. CopyResource will grab the latest for the GPU.
    D3D11_MAPPED_SUBRESOURCE map = {};
    hr = context->Map(
        MappedBuffer,
        0,
        D3D11_MAP_WRITE_NO_OVERWRITE,
        0,
        &map);
    OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "Map ring");
    PinnedMemory = (RingStruct*)map.pData;
    context->Unmap(MappedBuffer, 0);

    D3D11_BUFFER_DESC stagingBufferDesc = {};
    stagingBufferDesc.ByteWidth      = sizeof(RingStruct);
    stagingBufferDesc.Usage          = D3D11_USAGE_STAGING;
    //stagingBufferDesc.BindFlags    = 0;
    stagingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    StagingBuffer = nullptr;
    hr = device->CreateBuffer(&stagingBufferDesc, nullptr, &StagingBuffer.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "CreateBuffer staging");
    OVR_D3D_TAG_OBJECT(StagingBuffer);

    // Create constant buffer that will be read into from MappedBuffer.
    D3D11_BUFFER_DESC latchedBufferDesc = {};
    latchedBufferDesc.ByteWidth = sizeof(RingStruct);
    latchedBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    LatchedBuffer = nullptr;
    hr = device->CreateBuffer(&latchedBufferDesc, nullptr, &LatchedBuffer.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "CreateBuffer latched");
    OVR_D3D_TAG_OBJECT(LatchedBuffer);

    return OVRError::Success();
}

bool PoseLatch::IsInitialized() const
{
    return (PinnedMemory != nullptr);
}

void PoseLatch::PushPose(const Matrix4f leftEyeIn[2], const Matrix4f rightEyeIn[2],
                         double motionSensorTime, const double timewarpTimes[2])
{
    if (!IsInitialized())
    {
        OVR_ASSERT(false);
        return;
    }

    Lock::Locker locker(&CurrentFrameLock);

    ++UpdateSequence;

    int activeIndex = NextActiveIndex + 1;
    if (activeIndex >= RingElementCount)
    {
        activeIndex = 0;
    }
    NextActiveIndex = activeIndex;

    // Transpose matrices. SetUniform4x4Index transposes its matrix before
    // handing it over to D3D as a uniform. This is the reflection of that.
    Matrix4f leftEye[2] = { leftEyeIn[0], leftEyeIn[1] };
    Matrix4f rightEye[2] = { rightEyeIn[0], rightEyeIn[1] };
    for (int i = 0; i < 2; ++i)
    {
        leftEye[i].Transpose();
        rightEye[i].Transpose();
    }

    RingElement& element = PinnedMemory->RingElements[activeIndex];
    element.Start[0] = leftEye[0];  element.Start[1] = rightEye[0];
    element.End[0]   = leftEye[1];  element.End[1]   = rightEye[1];
    element.Debug.MotionSensorTime       = static_cast<float>(motionSensorTime);
    element.Debug.PredictedScanlineFirst = static_cast<float>(timewarpTimes[0]);
    element.Debug.Sequence               = UpdateSequence;
    element.Debug.Layer                  = CurrentLayer;

    // Don't allow read/write operations to move around this point
    MemoryBarrier();

    // Write the new active index. First index is the only one used.
    // Rest is pad.
    PinnedMemory->RingIndex[0] = activeIndex;

#ifdef OVR_LATCH_WRITE_ETW
    // Write ETW log containing debug data. Why always ring 0?
    ETW_WriteUpdate(element.Debug);
#endif
}

PoseLatch::DebugStruct PoseLatch::readStagingData(ID3D11DeviceContext* context)
{
    DebugStruct debug;
    HRESULT hr;

    D3D11_MAPPED_SUBRESOURCE stagingMap;
    hr = context->Map(
        StagingBuffer,
        0,
        D3D11_MAP_READ,
        0,
        &stagingMap);
    if (!OVR_D3D_CHECK(hr))
    {
        OVR_ASSERT(false);
        return DebugStruct();
    }

    RingStruct* debugMap = (RingStruct*)stagingMap.pData;

    // Look up the last written entry that was latched on GPU
    int ringIndex = (int)debugMap->RingIndex[0];
    debug = debugMap->RingElements[ringIndex].Debug;

    context->Unmap(StagingBuffer, 0);

    return debug;
}

bool PoseLatch::QueueLatchOnGPU(const Matrix4f       leftEye[2],
                                const Matrix4f       rightEye[2],
                                double               motionSensorTime,
                                const double         timewarpTimes[2],
                                uint32_t             cbSlot,
                                ID3D11DeviceContext* context)
{
    if (!IsInitialized())
    {
        OVR_ASSERT(false);
        return false;
    }

    Lock::Locker locker(&CurrentFrameLock);

    if (HavePriorStagingData)
    {
        // We must read the staging data before the StagingBuffer CopyResource
        // below. readStagingData maps the StagingBuffer and we will generate
        // a stall if we move the following line below the CopyResource which 
        // copies to the StagingBuffer below.
        DebugStruct result = readStagingData(context);

#ifdef OVR_LATCH_WRITE_ETW
        ETW_WriteLatchResult(result);
#endif

#ifdef OVR_MVP_LATCH_DUMP_PER_FRAME_STATS_TO_LOG
        LogError("Sequence = %d, Sensor-Time = %lf, Predicted TW Time = %lf"
                 result.Sequence,
                 result.MotionSensorTime,
                 result.PredictedTimewarp);
#endif
        OVR_UNUSED(result);
    }

    PushPose(leftEye, rightEye, motionSensorTime, timewarpTimes);

    context->CopyResource(LatchedBuffer, MappedBuffer);
    context->VSSetConstantBuffers(cbSlot, 1, &LatchedBuffer.GetRawRef());
    context->CopyResource(StagingBuffer, LatchedBuffer);

    HavePriorStagingData = true;

    return true;
}


}} // namespace OVR::CAPI
