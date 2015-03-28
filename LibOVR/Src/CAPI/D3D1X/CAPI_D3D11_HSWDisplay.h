/************************************************************************************

Filename    :   CAPI_D3D11_HSWDisplay.h
Content     :   Implements Health and Safety Warning system.
Created     :   July 7, 2014
Authors     :   Paul Pedriana

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

#ifndef OVR_CAPI_D3D11_HSWDisplay_h
#define OVR_CAPI_D3D11_HSWDisplay_h

#include "../CAPI_HSWDisplay.h"
#include "CAPI_D3D11_Util.h"

namespace OVR { namespace CAPI { namespace D3D11 {

class HSWDisplay : public CAPI::HSWDisplay
{
public:
    HSWDisplay(ovrRenderAPIType api, ovrHmd hmd, const HMDRenderState& renderState);

    // Must be called before use. apiConfig is such that:
    //   const ovrD3D11Config* config = (const ovrD3D11Config*)apiConfig; or
    bool Initialize(const ovrRenderAPIConfig* apiConfig);
    void Shutdown();
    void DisplayInternal();
    void DismissInternal();

    // Draws the warning to the eye texture(s). This must be done at the end of a 
    // frame but prior to executing the distortion rendering of the eye textures. 
    void RenderInternal(ovrEyeType eye, const ovrTexture* eyeTexture);

protected:
    void LoadGraphics();
    void UnloadGraphics();

    RenderParams                      RenderParams;
    Ptr<ID3D11SamplerState>           pSamplerState;
    Ptr<Texture>                      pTexture;
    Ptr<Buffer>                       pVB;
    Ptr<Buffer>                       UniformBufferArray[Shader_Count];
    Ptr<ShaderSet>                    pShaderSet;
    Ptr<ID3D11InputLayout>            pVertexInputLayout;
    Ptr<ID3D11BlendState>             pBlendState;
    Ptr<ID3D11RasterizerState>        pRasterizerState;
    Matrix4f                          OrthoProjection[ovrEye_Count];

private:
    OVR_NON_COPYABLE(HSWDisplay)
};

}}} // namespace OVR::CAPI::D3D11

#endif // OVR_CAPI_D3D11_HSWDisplay_h
