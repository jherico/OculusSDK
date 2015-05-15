/************************************************************************************

Filename    :   CAPI_PoseLatch.h
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

#ifndef OVR_CAPI_PoseLatch_h
#define OVR_CAPI_PoseLatch_h

#include <Kernel/OVR_Delegates.h>
#include <Extras/OVR_Math.h>
#include <OVR_Error.h>
#include <OVR_Stereo.h>
#if defined(OVR_OS_MS)
#include <Util/Util_Direct3D.h>
#endif

//#define OVR_MVP_LATCH_DUMP_PER_FRAME_STATS_TO_LOG
#if defined(OVR_OS_MS)
  #define OVR_LATCH_WRITE_ETW
#endif

namespace OVR { namespace CAPI {

class DistortionTimer;

//-----------------------------------------------------------------------------
// PoseLatch
//
// Implements late-latched pose matrices.
class PoseLatch
{
public:
    PoseLatch();
    virtual ~PoseLatch();

    /// Initializes GPU resources and pins memory. IsInitialized returns true
    /// after a successful call to this function.
    OVRError Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int layer);

    /// Returns true if Initialize has been called.
    bool IsInitialized() const;

    /// Queues a latch on the GPU from our pinned CPU memory.
    bool QueueLatchOnGPU(const Matrix4f       leftEye[2],
                         const Matrix4f       rightEye[2],
                         double               motionSensorTime,
                         const double         timewarpTimes[2],
                         uint32_t             cbSlot,
                         ID3D11DeviceContext* context);

    /// Writes latest pose into pinned memory.
    void PushPose(const Matrix4f leftEye[2],
                  const Matrix4f rightEye[2],
                  double motionSensorTime,
                  const double timewarpTimes[2]);

private:

    // We don't need to keep in-flight distortion data because we are careful
    // when we update the eye poses in the distortion renderer.

    // 2 + the number of forced updates per frame are required here.
    // Right now we just have one forced update from the render thread on
    // a new frame, so only 3 are required.
    // NOTE: Constant exists in the distortion shader.
    static const int RingElementCount = 3;

    struct DebugStruct
    {
        float     MotionSensorTime;       ///< Time at which IMU was sampled
        float     PredictedScanlineFirst; ///< Predicted time of first scanline
        uint32_t  Sequence;               ///< Sequence
        int32_t   Layer;                  ///< Associated layer
    };

    struct RingElement
    {
        Matrix4f Start[2], End[2];  ///< Start and end eye poses for (0) left and (1) right eye
        DebugStruct Debug;          ///< Debug data read from mapped StagingBuffer.
    };

    struct RingStruct
    {
        // First element of RingIndex is the only one used. The rest is pad
        // to make sure we align to 128 bits (a constant buffer requirement).
        uint32_t RingIndex[4];
        RingElement RingElements[RingElementCount];
    };

    DebugStruct readStagingData(ID3D11DeviceContext* context);
    
    Lock              CurrentFrameLock; ///< Lock for the Rings[] array and CurrentRingIndex update
    int               NextActiveIndex;  ///< Next ring element to write. Corresponds to index in RingElements struct.
    uint32_t          UpdateSequence;   ///< Sequence written. Protected by CurrentFrameLock.
    int               CurrentLayer;     ///< Associated layer. Used for debug purposes only.
    bool              HavePriorStagingData; ///< Have prior staging data.

    // The memory layout of all buffers is 'RingStruct'.

    // Memory-pinned data that IHVs say should not move under us.
    RingStruct*       PinnedMemory;   ///< Corresponds to memory mapped from 'MappedBuffer'.

    Ptr<ID3D11Buffer> MappedBuffer;   ///< Buffer whose memory is pinned at 'PinnedMemory'.
    Ptr<ID3D11Buffer> LatchedBuffer;  ///< Latched buffer actually used in the shader
    Ptr<ID3D11Buffer> StagingBuffer;  ///< Latched result read back into.

#ifdef OVR_LATCH_WRITE_ETW
    static void ETW_WriteUpdate(const DebugStruct& debug);
    static void ETW_WriteLatchResult(const DebugStruct& debugResult);
#endif

};


}} // OVR::CAPI

#endif

