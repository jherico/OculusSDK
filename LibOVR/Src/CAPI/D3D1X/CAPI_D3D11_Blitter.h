/************************************************************************************

Filename    :   CAPI_D3D11_Blitter.h
Content     :   D3D11 implementation for blitting, supporting scaling & rotation
Created     :   February 24, 2015
Authors     :   Reza Nourai

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

#ifndef OVR_CAPI_D3D11_Blitter_h
#define OVR_CAPI_D3D11_Blitter_h

#include "Kernel/OVR_RefCount.h"
#include "OVR_CAPI_D3D.h"
#include "OVR_Error.h"
#include "Compositor/Compositor_GraphicsContext.h"

#include <memory>

#ifdef OVR_OS_MS

#include <d3d11_1.h>

using OVR::Compositor::GraphicsContext;

namespace OVR { namespace CAPI {

//-------------------------------------------------------------------------------------
// ***** CAPI::Blitter

// D3D11 implementation of blitter

class Blitter : public RefCountBase<Blitter>
{
public:
    Blitter(const std::shared_ptr<GraphicsContext>& graphics);
    ~Blitter();

    OVRError Initialize();

    // Rotate flag allows Rotating the BLT clockwise, to correct for DK2-style Rift source
    bool Blt(ID3D11RenderTargetView* dest, ID3D11ShaderResourceView* source, int rotation = 0);

private:
    std::shared_ptr<GraphicsContext> Graphics;
    Ptr<ID3DDeviceContextState> BltState;
    Ptr<ID3D11InputLayout>      IL;
    Ptr<ID3D11Buffer>           VB;
    Ptr<ID3D11Buffer>           VBRotate;
    Ptr<ID3D11VertexShader>     VS;
    Ptr<ID3D11PixelShader>      PS;
    Ptr<ID3D11SamplerState>     Sampler;

    struct BltVertex
    {
        float x, y;
        float u, v;
    };
};

}} // namespace OVR::CAPI

#endif // OVR_OS_MS
#endif // OVR_CAPI_D3D11_Blitter_h
