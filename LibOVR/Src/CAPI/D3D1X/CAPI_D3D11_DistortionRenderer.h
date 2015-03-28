/************************************************************************************

Filename    :   CAPI_D3D11_DistortionRenderer.h
Content     :   Experimental distortion renderer
Created     :   November 11, 2013
Authors     :   Volga Aksoy

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

#ifndef OVR_CAPI_D3D11_DistortionRenderer_h
#define OVR_CAPI_D3D11_DistortionRenderer_h

#include "CAPI_D3D11_Util.h"
#include "../CAPI_DistortionRenderer.h"

#include "Kernel/OVR_Log.h"

namespace OVR { namespace CAPI { namespace D3D11 {


// ***** D3D11::DistortionRenderer

// Implementation of DistortionRenderer for D3D11.

class DistortionRenderer : public CAPI::DistortionRenderer
{
public:
    DistortionRenderer();
    ~DistortionRenderer();


    // Creation function for the device.    
    static CAPI::DistortionRenderer* Create();


    // ***** Public DistortionRenderer interface

    virtual void SubmitEye(int eyeId, const ovrTexture* eyeTexture) OVR_OVERRIDE;
    virtual void SubmitEyeWithDepth(int eyeId, const ovrTexture* eyeColorTexture, const ovrTexture* eyeDepthTexture) OVR_OVERRIDE;

    virtual void EndFrame(uint32_t frameIndex, bool swapBuffers);

    // TBD: Make public?
    void         WaitUntilGpuIdle();

    // Similar to ovr_WaitTillTime but it also flushes GPU.
    // Note, it exits when time expires, even if GPU is not in idle state yet.
    double       FlushGpuAndWaitTillTime(double absTime);

protected:
    virtual bool initializeRenderer(const ovrRenderAPIConfig* apiConfig) OVR_OVERRIDE;

    class GraphicsState : public CAPI::DistortionRenderer::GraphicsState
    {
    public:
        GraphicsState(ID3D11DeviceContext* context);
        virtual ~GraphicsState();
        virtual void clearMemory();
        virtual void Save();
        virtual void Restore();

    protected:
        ID3D11DeviceContext* context;
        BOOL memoryCleared;

        ID3D11RasterizerState* rasterizerState;
        ID3D11InputLayout* inputLayoutState;

        ID3D11ShaderResourceView*   psShaderResourceState[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
        ID3D11SamplerState*         psSamplerStates[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
        ID3D11Buffer*               psConstantBuffersState[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

        ID3D11ShaderResourceView*   vsShaderResourceState[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
        ID3D11SamplerState*         vsSamplerStates[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
        ID3D11Buffer*               vsConstantBuffersState[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

        ID3D11ShaderResourceView*   csShaderResourceState[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
        ID3D11SamplerState*         csSamplerStates[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
        ID3D11Buffer*               csConstantBuffersState[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
        ID3D11UnorderedAccessView*  csUnorderedAccessViewState[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];

        ID3D11RenderTargetView* renderTargetViewState[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
        ID3D11DepthStencilView* depthStencilViewState;

        ID3D11BlendState* omBlendState;
        FLOAT omBlendFactorState[4];
        UINT omSampleMaskState;

        D3D11_PRIMITIVE_TOPOLOGY primitiveTopologyState;

        ID3D11Buffer* iaIndexBufferPointerState;
        DXGI_FORMAT iaIndexBufferFormatState;
        UINT iaIndexBufferOffsetState;

        ID3D11Buffer* iaVertexBufferPointersState[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        UINT iaVertexBufferStridesState[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        UINT iaVertexBufferOffsetsState[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

        ID3D11PixelShader* currentPixelShader;
        ID3D11VertexShader* currentVertexShader;
        ID3D11GeometryShader* currentGeometryShader;
        ID3D11HullShader* currentHullShader;
        ID3D11DomainShader* currentDomainShader;
        ID3D11ComputeShader* currentComputeShader;
    };

private:
    // Helpers
    bool initBuffersAndShaders();
    void initShaders();
    void initFullscreenQuad();
    void initOverdrive();
    void destroy();

    void setViewport(const Recti& vp);

    void renderDistortion();

    void renderPrimitives(const ShaderFill* fill, Buffer* vertices, Buffer* indices,
        Matrix4f* viewMatrix, int offset, int count,
        PrimitiveType rprim);

    void renderEndFrame();

    void createDrawQuad();
    void renderLatencyQuad(unsigned char* latencyTesterDrawColor);
    void renderLatencyPixel(unsigned char* latencyTesterPixelColor);

    // Attempt to use DXGI GetFrameStatistics for getting a previous vsync
    // Returns 0 if no Vsync timing information is available.
    double getDXGILastVsyncTime();

    // Create or get cached D3D sampler based on flags.
    ID3D11SamplerState* getSamplerState(int sm);


    //// TBD: Should we be using oe from RState instead?
    //unsigned            DistortionCaps;

    // Back buffer is properly set as an SRGB format?
    bool                SrgbBackBuffer;

    // Failures retrieving the frame index from renderer
    int                 FrameIndexFailureCount;
    static const int    FrameIndexFailureLimit = 5; // After a few failures stop trying.

    // D3DX device and utility variables.
    RenderParams        RParams;
    Ptr<Texture>        pEyeTextures[2];
    Ptr<Texture>        pEyeDepthTextures[2];

    // U,V scale and offset needed for timewarp.
    ovrVector2f         UVScaleOffset[2][2];
    ovrSizei            EyeTextureSize[2];
    ovrRecti            EyeRenderViewport[2];

    Ptr<Texture>        pOverdriveTextures[NumOverdriveTextures];
    Ptr<Texture>        OverdriveLutTexture;

    //Ptr<Buffer>         mpFullScreenVertexBuffer;

    Ptr<Buffer>         DistortionMeshVBs[2];    // one per-eye
    Ptr<Buffer>         DistortionMeshIBs[2];    // one per-eye
    Ptr<Buffer>         DistortionPinBuffer[2];  // one per-eye

    Ptr<ShaderSet>      DistortionShader;
    Ptr<ID3D11InputLayout> DistortionVertexIL;

    struct StandardUniformData
    {
        Matrix4f  Proj;
        Matrix4f  View;
    }                   StdUniforms;
    Ptr<Buffer>         UniformBuffers[Shader_Count];

    Ptr<ID3D11SamplerState>     SamplerStates[Sample_Count];
    Ptr<ID3D11RasterizerState>  Rasterizer;

    Ptr<Buffer>         LatencyTesterQuadVB;
    Ptr<ShaderSet>      SimpleQuadShader;
    Ptr<ID3D11InputLayout> SimpleQuadVertexIL;

    GpuTimer GpuProfiler;
    Hash<ID3D11Texture2D*, Ptr<ID3D11RenderTargetView>> RenderTargetMap;
};

}}} // OVR::CAPI::D3D11

#endif // OVR_CAPI_D3D11_DistortionRenderer_h
