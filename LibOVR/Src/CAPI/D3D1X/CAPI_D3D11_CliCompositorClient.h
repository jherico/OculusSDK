/************************************************************************************

Filename    :   CAPI_D3D11_CliCompositorClient.h
Content     :   D3D11 implementation for client connection to the compositor service.
Created     :   December 16, 2014
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

#ifndef OVR_CAPI_D3D11_CliCompositorClient_h
#define OVR_CAPI_D3D11_CliCompositorClient_h

#include "../CAPI_CliCompositorClient.h"
#include "OVR_CAPI_D3D.h"

#include "../../LibOVRKernel/Src/GL/CAPI_GLE.h"
#include "OVR_CAPI_GL.h"

#include <memory>

#ifdef OVR_OS_MS

namespace OVR { namespace CAPI {

struct GLTextureInterop
{
    HANDLE hDevice;
    GLuint TexId;
    HANDLE InteropHandle;
    BOOL Locked;

    GLTextureInterop(HANDLE hDevice);
    ~GLTextureInterop();

    void Lock();
    void Unlock();
};

struct TextureSet
{
    // NOTE NOTE! This MUST be first member of the struct!
    // Public facing part of the API object.
    // We return a pointer to this member to the application
    ovrSwapTextureSet   AppInfo;

    // Service-provided unique ID. Used in all calls to the service
    uint32_t            ID;

    // Only used when doing GL/D3D11 interop to enable GL client applications on Windows
    std::vector<std::shared_ptr<GLTextureInterop>> GL_Textures;

    TextureSet();
    ~TextureSet();
};

//-------------------------------------------------------------------------------------
// ***** CAPI::CliD3D11CompositorClient

// D3D11 implementation of client connection to the compositor service.

class CliD3D11CompositorClient : public CliCompositorClient
{
public:
    CliD3D11CompositorClient(HMDState const* hmdState);
    ~CliD3D11CompositorClient();

    // For D3D
    OVRError CreateTextureSetD3D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC* desc, ovrSwapTextureSet** outTextureSet);
    OVRError CreateMirrorTextureD3D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC* desc, ovrTexture** outMirrorTexture);

    // For GL on Windows
    OVRError CreateTextureSetGL(GLuint format, int width, int height, ovrSwapTextureSet** outTextureSet);
    OVRError CreateMirrorTextureGL(GLuint format, int width, int height, ovrTexture** outMirrorTexture);

    // Destroy a texture set, freeing all the resources
    virtual OVRError DestroyTextureSet(ovrSwapTextureSet* textureSet) OVR_OVERRIDE;

    // Support shared mirror texture
    virtual OVRError DestroyMirrorTexture(ovrTexture* mirrorTexture) OVR_OVERRIDE;

    // Layer manipulation
    virtual OVRError SubmitLayer(int layerNum, LayerDesc const *layerDesc) OVR_OVERRIDE;
    virtual OVRError DisableLayer(int layerNum) OVR_OVERRIDE;

    // Complete the frame, finalize submissions, synchronize with compositor service.
    virtual OVRError EndFrame(uint32_t appFrameIndex, ovrViewScaleDesc const *viewScaleDesc) OVR_OVERRIDE;

    virtual OVRError SetQueueAheadSeconds(float queueAheadSeconds) OVR_OVERRIDE;
    virtual float GetQueueAheadSeconds() const OVR_OVERRIDE
    {
        return QueueAheadSeconds;
    }

private:
    OVRError initialize(ID3D11Device* device);

    // Called on texture set destruction. Should unwind our
    // D3D11 bindings in the event there are no further texture sets
    // that reference our device. Will also break the compositor
    // connection if no active device references exist
    OVRError uninitialize();

    OVRError createTextureSetInternal(const D3D11_TEXTURE2D_DESC* desc,
                                      std::shared_ptr<TextureSet>& textureSetData,
                                      std::vector<Ptr<ID3D11Texture2D>>& textures);

    Service::CompositorLayerDesc *findOrCreateLayerDesc(int layerNum);

private:
    ovrRenderAPIType            ClientRenderAPI;
    bool                        Initialized;

    /// D3D11-client data
    Ptr<ID3D11Device>           D3D11_Device;
    Ptr<ID3D11DeviceContext>    D3D11_Context;
    Ptr<ID3D11Texture2D>        D3D11_MirrorTexture;
    Ptr<ID3D11ShaderResourceView> D3D11_MirrorTextureSRV;
    // When DeviceReferences becomes 0, we should remove our reference to our D3D11 Device and Context
    int                         DeviceReferences; 

    /// OpenGL-client data
    HANDLE                      GL_hDevice;
    std::unique_ptr<GLTextureInterop> GL_MirrorTexture;

    /// Synchronization data
    ScopedSemaphoreHANDLE       FrameQueueSemaphore;
    Ptr<IDXGIKeyedMutex>        Fence;
    float                       QueueAheadSeconds;  // between 0 and FrameInterval

    /// List of all active TextureSets
    std::vector<std::shared_ptr<TextureSet>> TextureSets;

    /// List of all currently-unlocked GL texture set textures. See SubmitFrame for more details.
    std::vector<std::shared_ptr<GLTextureInterop>> UnlockedGlTextures;

    // Queued up over a frame by findOrCreateLayerDesc() and flushed to the server by EndFrame()
    ArrayPOD<Service::CompositorLayerDesc> CompositorLayers;
    TextureSet* CompositorTextureSets[MaxNumLayersPublic][4];       // Two colour, two depth.

};

}} // namespace OVR::CAPI

#endif // OVR_OS_MS
#endif // OVR_CAPI_D3D11_CliCompositorClient_h
