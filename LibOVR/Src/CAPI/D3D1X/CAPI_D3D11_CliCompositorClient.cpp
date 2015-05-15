/************************************************************************************

Filename    :   CAPI_D3D11_CompositorClient.cpp
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

#include "CAPI_D3D11_CliCompositorClient.h"
#include "../CAPI_HMDState.h"

#include "../GL/CAPI_GL_Util.h"
#include "Tracing/LibOVREvents.h"
#include "Tracing/Tracing.h"

#ifdef OVR_OS_MS

namespace OVR { namespace CAPI {

#ifdef OVR_BUILD_DEBUG
static const uint32_t SynchronizationTimeoutMs = INFINITE;
#else
static const uint32_t SynchronizationTimeoutMs = 1000; // FIXME: 1 second in production
#endif


#define AssertAndReturnErrorOnFail(exp, err) { if(FAILED(exp)) { OVR_ASSERT(false); return err;} }

static void DXGIFormatFromGLFormat(GLint format, DXGI_FORMAT *Out_pDXFormat, UINT *Out_pDXBindFlags )
{
    *Out_pDXBindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    switch (format)
    {
    case GL_BGRA:               *Out_pDXFormat = DXGI_FORMAT_B8G8R8A8_UNORM;        break;
    case GL_RGBA:               *Out_pDXFormat = DXGI_FORMAT_R8G8B8A8_UNORM;        break;
    case GL_SRGB8_ALPHA8:       *Out_pDXFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;   break;
    case GL_DEPTH_COMPONENT32:
    case GL_DEPTH_COMPONENT32F:
        // DXGI_FORMAT_D32_FLOAT will be converted to R32_TYPELESS inside createTextureSetInternal(),
        // but that function needs to know it really is a depth texture, so we keep it D32 here.
        *Out_pDXFormat = DXGI_FORMAT_D32_FLOAT;
        *Out_pDXBindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
        break;

    case GL_DEPTH_COMPONENT24:
        // DXGI_FORMAT_D24_UNORM_S8_UINT will be converted to DXGI_FORMAT_R24G8_TYPELESS inside createTextureSetInternal(),
        // but that function needs to know it really is a depth texture, so we keep it D24 here.
        *Out_pDXFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        *Out_pDXBindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
        break;

    default:
        // Untested!
        OVR_ASSERT(false);
        *Out_pDXFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

//-------------------------------------------------------------------------------------
// ***** GLTextureInterop

GLTextureInterop::GLTextureInterop(HANDLE hDevice)
    : hDevice(hDevice)
    , TexId(0)
    , InteropHandle(nullptr)
    , Locked(FALSE)
{
}

GLTextureInterop::~GLTextureInterop()
{
    if (Locked)
        Unlock();
    if (InteropHandle)
        OVR_ASSERT(wglDXUnregisterObjectNV(hDevice, InteropHandle));
    if (TexId)
        glDeleteTextures(1, &TexId);
}

void GLTextureInterop::Lock()
{
    OVR_ASSERT ( !Locked );
    BOOL GLTextureInterop_Lock_Ok = wglDXLockObjectsNV(hDevice, 1, &InteropHandle);
    OVR_ASSERT(GLTextureInterop_Lock_Ok);
    if (GLTextureInterop_Lock_Ok)
        Locked = TRUE;
}

void GLTextureInterop::Unlock()
{
    // flush GL pipe to make sure texture contents will be updated before D3D takes over
    glFlush();

    OVR_ASSERT ( Locked );        
    BOOL GLTextureInterop_Unlock_Ok = wglDXUnlockObjectsNV(hDevice, 1, &InteropHandle);
    OVR_ASSERT(GLTextureInterop_Unlock_Ok);
    if (GLTextureInterop_Unlock_Ok)
        Locked = FALSE;
}

//-------------------------------------------------------------------------------------
// ***** TextureSet

TextureSet::TextureSet()
    : ID(CliCompositorClient::InvalidTextureSetID)
{
    memset(&AppInfo, 0, sizeof(AppInfo));
}

TextureSet::~TextureSet()
{
    if (AppInfo.Textures)
    {
        // For D3D11, the ovrTexture is holding the only reference to the texture object
        // For GL, the DX/GL interop layer holds the reference, and when we unregister the
        // objects in the dtor of the texture interop wrappers, the reference is released

        if (GL_Textures.empty())    // D3D11
        {
            for (int i = 0; i < AppInfo.TextureCount; ++i)
            {
                ovrD3D11Texture* tex = (ovrD3D11Texture*)&AppInfo.Textures[i];
                tex->D3D11.pTexture->Release();
            }
        }

        delete[] AppInfo.Textures;
        AppInfo.Textures = nullptr;
    }
}


//-------------------------------------------------------------------------------------
// ***** CAPI::D3D11CompositorClient

CliD3D11CompositorClient::CliD3D11CompositorClient(HMDState const* hmdState)
    : CliCompositorClient(hmdState)
    , ClientRenderAPI(ovrRenderAPI_None)
    , Initialized(false)
    , GL_hDevice(nullptr)
    , QueueAheadSeconds(0.f)
    , DeviceReferences(0)
{
    OVR_ASSERT(HmdState && HmdState->pClient);

    CompositorLayers.Reserve ( MaxNumLayersPublic );
    Service::CompositorLayerDesc descDisabled;
    descDisabled.Desc.SetToDisabled();
    descDisabled.TextureSetIDColor[0] = InvalidTextureSetID;
    descDisabled.TextureSetIDColor[1] = InvalidTextureSetID;
    descDisabled.TextureSetIDDepth[0] = InvalidTextureSetID;
    descDisabled.TextureSetIDDepth[1] = InvalidTextureSetID;
    for ( int i = 0; i < MaxNumLayersPublic; i++ )
    {
        descDisabled.LayerNum = i;
        CompositorLayers.PushBack ( descDisabled );
    }
}

CliD3D11CompositorClient::~CliD3D11CompositorClient()
{
    // All TextureSets should have been destroyed by now
    OVR_ASSERT(TextureSets.empty());
    OVR_ASSERT(UnlockedGlTextures.empty());

    // In case the app didn't destroy them, delete them all now.
    // This will render their ovrSwapTextureSet* pointers invalid.
    TextureSets.clear();
    UnlockedGlTextures.clear();
    CompositorLayers.Clear();

    // Clean up server side of mirroring if it's still enabled
    compDestroyMirrorTexture();

    // Detach interop device
    if (GL_hDevice)
    {
        wglDXCloseDeviceNV(GL_hDevice);
        GL_hDevice = nullptr;
    }
}

OVRError CliD3D11CompositorClient::CreateTextureSetD3D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC* desc, ovrSwapTextureSet** outTextureSet)
{
    // initialize no-ops if already initialized
    OVRError err = initialize(device);
    if (!err.Succeeded())
    {
        return err;
    }

    // Initialize out parameter to null in case we exit on error
    *outTextureSet = nullptr;

    std::shared_ptr<TextureSet> textureSet = std::make_shared<TextureSet>();
    std::vector<Ptr<ID3D11Texture2D>> textures;

    err = createTextureSetInternal(desc, textureSet, textures);
    if (!err.Succeeded())
    {
        return err;
    }

    int texCount = (int)textures.size();

    textureSet->AppInfo.TextureCount = texCount;
    textureSet->AppInfo.Textures = new ovrTexture[texCount];
    memset(textureSet->AppInfo.Textures, 0, sizeof(ovrTexture) * texCount);

    for (int i = 0; i < texCount; ++i)
    {
        ovrD3D11Texture* tex = (ovrD3D11Texture*)&(textureSet->AppInfo.Textures[i]);
        tex->D3D11.Header.API = ovrRenderAPI_D3D11;
        tex->D3D11.Header.TextureSize = Sizei(desc->Width, desc->Height);
        tex->D3D11.pTexture = textures[i];

        // Ownership has been handed off to the ovrTexture
        textures[i].NullWithoutRelease();
    }

    // It's official! Add it to TextureSets
    TextureSets.push_back(textureSet);

    *outTextureSet = &textureSet->AppInfo;

    return OVRError::Success();
}

OVRError CliD3D11CompositorClient::CreateMirrorTextureD3D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC* desc, ovrTexture** outMirrorTexture)
{
    // initialize no-ops if already initialized
    OVRError err = initialize(device);
    if (!err.Succeeded())
    {
        return err;
    }

    if (D3D11_MirrorTexture)
    {
        // Already have a mirror, not valid to reinit the texture
        return OVR_MAKE_ERROR(ovrError_Reinitialization, "There's already a mirror texture active.");
    }

    D3D11_TEXTURE2D_DESC td = *desc;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    Ptr<ID3D11Texture2D> texture;
    OVR_D3D_CREATE(texture, D3D11_Device->CreateTexture2D(&td, nullptr, &texture.GetRawRef()));

    Ptr<IDXGIResource1> resource;
    HRESULT hr = texture->QueryInterface(IID_PPV_ARGS(&resource.GetRawRef()));
    OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "QueryInterface IDXGIResource1");

    HANDLE textureHandle;
    hr = resource->GetSharedHandle(&textureHandle);
    OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "GetSharedHandle texture");

    err = compCreateMirrorTexture(reinterpret_cast<handle64_t>(textureHandle));
    if (!err.Succeeded())
    {
        return err;
    }

    OVR_D3D_CREATE(D3D11_MirrorTextureSRV, D3D11_Device->CreateShaderResourceView(texture, nullptr, &D3D11_MirrorTextureSRV.GetRawRef()));

    D3D11_MirrorTexture = texture;

    ovrD3D11Texture* tex = new ovrD3D11Texture;
    OVR_ASSERT(outMirrorTexture); // Else we leak
    if (outMirrorTexture)
    {
        *outMirrorTexture = (ovrTexture*)tex;
    }

    tex->D3D11.Header.API = ovrRenderAPI_D3D11;
    tex->D3D11.Header.TextureSize = Sizei(td.Width, td.Height);
    tex->D3D11.pTexture = D3D11_MirrorTexture.GetPtr();
    tex->D3D11.pSRView = D3D11_MirrorTextureSRV.GetPtr();

    return OVRError::Success();
}

OVRError CliD3D11CompositorClient::CreateTextureSetGL(GLuint format, int width, int height, ovrSwapTextureSet** outTextureSet)
{
    // initialize no-ops if already initialized
    OVRError err = initialize(nullptr);
    if (!err.Succeeded())
    {
        return err;
    }

    // Initialize out parameter to null in case we exit on error
    OVR_ASSERT(outTextureSet);
    *outTextureSet = nullptr;

    D3D11_TEXTURE2D_DESC td = {};
    td.ArraySize = 1;
    DXGIFormatFromGLFormat(format, &td.Format, &td.BindFlags);
    td.MipLevels = 1;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.Width = width;
    td.Height = height;
    td.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    std::shared_ptr<TextureSet> textureSet = std::make_shared<TextureSet>();
    std::vector<Ptr<ID3D11Texture2D>> textures;

    err = createTextureSetInternal(&td, textureSet, textures);
    if (!err.Succeeded())
    {
        return err;
    }

    int texCount = (int)textures.size();

    textureSet->AppInfo.TextureCount = texCount;
    textureSet->AppInfo.Textures = new ovrTexture[texCount];
    memset(textureSet->AppInfo.Textures, 0, sizeof(ovrTexture)* texCount);

    for (int i = 0; i < texCount; ++i)
    {
        std::shared_ptr<GLTextureInterop> glTexture = std::make_shared<GLTextureInterop>(GL_hDevice);

        glGenTextures(1, &glTexture->TexId);

        // The DX/GL interop layer takes a reference to the texture object, and when we unregister the
        // objects in the dtor of the texture set data, the reference is released. This keeps the texture
        // object valid even when the textures vector goes away
        glTexture->InteropHandle = wglDXRegisterObjectNV(GL_hDevice, textures[i].GetPtr(),
                                                         glTexture->TexId, GL_TEXTURE_2D,
                                                         WGL_ACCESS_READ_WRITE_NV);

        if (!glTexture->InteropHandle)
        {
            return OVR_MAKE_SYS_ERROR(ovrError_Initialize, glGetError(), "wglDXRegisterObjectNV failed");
        }

        // Store the interop texture in the texture Set
        textureSet->GL_Textures.push_back(glTexture);

        // Start all surfaces "locked", meaning the GL app can use them for rendering.
        // See the comment block in EndFrame below about how this lock/unlock system works in GL.
        glTexture->Lock();

        ovrGLTexture* tex = (ovrGLTexture*)&(textureSet->AppInfo.Textures[i]);
        tex->OGL.Header.API = ovrRenderAPI_OpenGL;
        tex->OGL.Header.TextureSize = Sizei(td.Width, td.Height);
        tex->OGL.TexId = textureSet->GL_Textures[i]->TexId;
    }

    // It's official! Add it to TextureSets
    TextureSets.push_back(textureSet);

    *outTextureSet = &textureSet->AppInfo;

    return ovrSuccess;
}

OVRError CliD3D11CompositorClient::CreateMirrorTextureGL(GLuint format, int width, int height, ovrTexture** outMirrorTexture)
{
    // initialize no-ops if already initialized
    OVRError err = initialize(nullptr);
    if (!err.Succeeded())
    {
        return err;
    }

    if (D3D11_MirrorTexture || GL_MirrorTexture)
    {
        // Already have a mirror, not valid to reinit the texture
        return OVR_MAKE_ERROR(ovrError_Reinitialization, "There's already a mirror texture active.");
    }

    OVR_ASSERT(outMirrorTexture);
    *outMirrorTexture = nullptr;

    D3D11_TEXTURE2D_DESC td = {};
    td.ArraySize = 1;
    DXGIFormatFromGLFormat ( format, &td.Format, &td.BindFlags );
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    Ptr<ID3D11Texture2D> texture;
    OVR_D3D_CREATE(texture, D3D11_Device->CreateTexture2D(&td, nullptr, &texture.GetRawRef()));

    Ptr<IDXGIResource1> resource;
    OVR_D3D_CREATE_NOTAG(resource, texture->QueryInterface(IID_PPV_ARGS(&resource.GetRawRef())));

    HANDLE textureHandle;
    HRESULT hr = resource->GetSharedHandle(&textureHandle);
    OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "GetSharedHandle texture");

    err = compCreateMirrorTexture(reinterpret_cast<handle64_t>(textureHandle));
    if (!err.Succeeded())
    {
        return err;
    }

    OVR_D3D_CREATE(D3D11_MirrorTextureSRV, D3D11_Device->CreateShaderResourceView(texture, nullptr, &D3D11_MirrorTextureSRV.GetRawRef()));

    D3D11_MirrorTexture = texture;

    GL_MirrorTexture.reset(new GLTextureInterop(GL_hDevice));
    glGenTextures(1, &GL_MirrorTexture->TexId);

    GL_MirrorTexture->InteropHandle = wglDXRegisterObjectNV(GL_hDevice, texture.GetPtr(),
                                                            GL_MirrorTexture->TexId, GL_TEXTURE_2D,
                                                            WGL_ACCESS_READ_WRITE_NV);

    if (!GL_MirrorTexture->InteropHandle)
    {
        return OVR_MAKE_SYS_ERROR(ovrError_Initialize, glGetError(), "wglDXRegisterObjectNV failed");
    }

    GL_MirrorTexture->Lock();

    // now create the ovrTexture
    ovrGLTexture* tex = new ovrGLTexture;
    *outMirrorTexture = (ovrTexture*)tex;

    tex->OGL.Header.API = ovrRenderAPI_OpenGL;
    tex->OGL.Header.TextureSize = Sizei(width, height);
    tex->OGL.TexId = GL_MirrorTexture->TexId;

    return OVRError::Success();
}

/*virtual*/ OVRError CliD3D11CompositorClient::DestroyTextureSet(ovrSwapTextureSet* textureSet)
{
    if (!Initialized)
    {
        return OVR_MAKE_ERROR(ovrError_NotInitialized, "Not initialized");
    }

    TextureSet* set = (TextureSet*)textureSet;

    // Destroy service side
    OVRError err = compDestroyTextureSet(set->ID);
    if (!err.Succeeded())
    {
        return err;
    }

    if (ClientRenderAPI == ovrRenderAPI_OpenGL)
    {
        for (auto pglText : set->GL_Textures)
        {
            auto it = std::find ( UnlockedGlTextures.begin(), UnlockedGlTextures.end(), pglText );
            if ( it != UnlockedGlTextures.end() )
            {
                // Remove it from the list.
                // No need to relock it - the GLTextureInterop is keeping track.
                OVR_ASSERT ( !(pglText)->Locked );
                UnlockedGlTextures.erase ( it );
                break;
            }
        }
    }

    // Remove from list, which removes last reference to shared pointer and deletes the object
    for (auto it = TextureSets.begin(); it != TextureSets.end(); ++it)
    {
        if ((*it)->ID == set->ID)
        {
            TextureSets.erase(it);
            break;
        }
    }

    if (ClientRenderAPI == ovrRenderAPI_D3D11)
        return uninitialize();

    return OVRError::Success();
}

/*virtual*/ OVRError CliD3D11CompositorClient::DestroyMirrorTexture(ovrTexture* mirrorTexture)
{
    if (!Initialized)
    {
        return OVR_MAKE_ERROR(ovrError_NotInitialized, "Not initialized");
    }

    if (mirrorTexture->Header.API == ovrRenderAPI_D3D11)
    {
        ovrD3D11Texture* tex = (ovrD3D11Texture*)mirrorTexture;

        // Is this the right mirror texture?
        if (tex->D3D11.pTexture != D3D11_MirrorTexture.GetPtr())
        {
            return OVR_MAKE_ERROR_F(ovrError_ServiceError, "Wrong mirror texture %p != %p", tex->D3D11.pTexture, D3D11_MirrorTexture.GetPtr());
        }

        D3D11_MirrorTexture = nullptr;
        D3D11_MirrorTextureSRV = nullptr;

        delete mirrorTexture;
    }
    else if (mirrorTexture->Header.API == ovrRenderAPI_OpenGL)
    {
        ovrGLTexture* tex = (ovrGLTexture*)mirrorTexture;

        // Is this the right texture?
        if (tex->OGL.TexId != GL_MirrorTexture->TexId)
        {
            return OVR_MAKE_ERROR_F(ovrError_ServiceError, "Wrong texture %d != %d", (int)tex->OGL.TexId, (int)GL_MirrorTexture->TexId);
        }

        D3D11_MirrorTexture = nullptr;
        D3D11_MirrorTextureSRV = nullptr;
        GL_MirrorTexture.reset();

        delete mirrorTexture;
    }
    else
    {
        return OVR_MAKE_ERROR(ovrError_ServiceError, "No API");
    }

    // Release the server side

    OVRError err = compDestroyMirrorTexture();

    // Unwind D3D11 device references
    if (err.Succeeded() && ClientRenderAPI == ovrRenderAPI_D3D11)
        err = uninitialize();

    return err;
}

/*virtual*/ OVRError CliD3D11CompositorClient::SubmitLayer(int layerNum, LayerDesc const *layerDesc)
{
    if (!Initialized)
    {
        return OVR_MAKE_ERROR(ovrError_NotInitialized, "Not initialized");
    }

    CompositorLayerDesc &compLayerDesc = *findOrCreateLayerDesc(layerNum);
    compLayerDesc.LayerNum = layerNum;
    compLayerDesc.Desc = *layerDesc;
    OVR_ASSERT(compLayerDesc.Desc.Type != LayerDesc::LayerType_Disabled);

    // color texture sets
    for (int i = 0; i < 2; ++i)
    {
        compLayerDesc.TextureSetIDColor[i] = InvalidTextureSetID;

        TextureSet* set = (TextureSet*)layerDesc->pEyeTextureSets[i];
        CompositorTextureSets[layerNum][i] = set;
        if (set != nullptr)
        {
            int curIndex = set->AppInfo.CurrentIndex;
            if ( ( curIndex < 0 ) || ( curIndex >= set->AppInfo.TextureCount ) )
            {
                OVR_ASSERT ( false );
                curIndex = 0;
            }
            compLayerDesc.TextureSetIDColor[i] = set->ID;
            compLayerDesc.TextureIndexColor[i] = curIndex;

            // OGL lock/unlock not handled here - all done in EndFrame.
        }
        else if (i == 1)    // if second index is null, just reuse first index's data
        {
            compLayerDesc.TextureSetIDColor[i] = compLayerDesc.TextureSetIDColor[0];
            compLayerDesc.TextureIndexColor[i] = compLayerDesc.TextureIndexColor[0];
        }
    }

    // depth texture sets
    for (int i = 0; i < 2; ++i)
    {
        compLayerDesc.TextureSetIDDepth[i] = InvalidTextureSetID;

        TextureSet* set = (TextureSet*)layerDesc->pEyeDepthTextureSets[i];
        CompositorTextureSets[layerNum][i+2] = set;     // Array entries are 0&1 = color, 2&3 = depth.
        if (set != nullptr)
        {
            int curIndex = set->AppInfo.CurrentIndex;
            if ( ( curIndex < 0 ) || ( curIndex >= set->AppInfo.TextureCount ) )
            {
                OVR_ASSERT ( false );
                curIndex = 0;
            }
            compLayerDesc.TextureSetIDDepth[i] = set->ID;
            compLayerDesc.TextureIndexDepth[i] = curIndex;

            // OGL lock/unlock not handled here - all done in EndFrame.
        }
        else if (i == 1)    // if second index is null, just reuse first index's data
        {
            compLayerDesc.TextureSetIDDepth[i] = compLayerDesc.TextureSetIDDepth[0];
            compLayerDesc.TextureIndexDepth[i] = compLayerDesc.TextureIndexDepth[0];
        }
    }

    return OVRError::Success();
}

/*virtual*/ OVRError CliD3D11CompositorClient::DisableLayer(int layerNum)
{
    if (!Initialized)
    {
        return OVR_MAKE_ERROR(ovrError_NotInitialized, "Not initialized");
    }

    CompositorLayerDesc *compLayerDesc = findOrCreateLayerDesc(layerNum);
    compLayerDesc->Desc.SetToDisabled();
    compLayerDesc->LayerNum = layerNum;
    for ( int eye = 0; eye < 2; eye++ )
    {
        compLayerDesc->TextureSetIDColor[eye] = InvalidTextureSetID;
        compLayerDesc->TextureSetIDDepth[eye] = InvalidTextureSetID;

        CompositorTextureSets[layerNum][eye] = nullptr;
        CompositorTextureSets[layerNum][eye+2] = nullptr;
    }
    // ...and we actually send the layer to the server at EndFrame.

    return OVRError::Success();
}


/*virtual*/ OVRError CliD3D11CompositorClient::EndFrame(uint32_t appFrameIndex, ovrViewScaleDesc const *viewScaleDesc)
{
    TraceCall(appFrameIndex);
    if (!Initialized)
    {
        return OVR_MAKE_ERROR(ovrError_NotInitialized, "Not initialized");
    }

#ifdef OVR_BUILD_DEBUG
    // Paranoia check. Make sure our various representations all agree.
    OVR_ASSERT ( CompositorLayers.GetSizeI() == OVR_ARRAY_COUNT(CompositorTextureSets) );
    for ( int layerNum = 0; layerNum < OVR_ARRAY_COUNT(CompositorTextureSets); layerNum++ )
    {
        Service::CompositorLayerDesc *layer = &(CompositorLayers[layerNum]);
        for ( int eyeNum = 0; eyeNum < 2; eyeNum++ )
        {
            TextureSet *set = CompositorTextureSets[layerNum][eyeNum];
            if ( set != nullptr )
            {
                OVR_ASSERT ( layer->Desc.Type != LayerDesc::LayerType_Disabled );
                OVR_ASSERT ( layer->TextureSetIDColor[eyeNum] == set->ID );
            }
            else
            {
                OVR_ASSERT ( layer->Desc.Type == LayerDesc::LayerType_Disabled );
                OVR_ASSERT ( layer->TextureSetIDColor[eyeNum] == InvalidTextureSetID );
            }

            set = CompositorTextureSets[layerNum][eyeNum+2];
            if ( set != nullptr )
            {
                OVR_ASSERT ( layer->Desc.Type == LayerDesc::LayerType_FovWithDepth );
                OVR_ASSERT ( layer->TextureSetIDDepth[eyeNum] == set->ID );
            }
            else
            {
                OVR_ASSERT ( layer->Desc.Type != LayerDesc::LayerType_FovWithDepth );
                OVR_ASSERT ( layer->TextureSetIDDepth[eyeNum] == InvalidTextureSetID );
            }
        }
    }
#endif

    if (ClientRenderAPI == ovrRenderAPI_OpenGL)
    {
        // For GL sets, we by default keep all the textures locked.
        // From D3D's point of view, "locked" means that "something else"
        // (in this case GL) can use them. So by default everything is
        // available for use by GL.
        //
        // However, we're just about to send a bunch for composition by D3D, so
        // we need to unlock them so D3D can use them, then re-lock them after use.
        //
        // However, if we did:
        //
        // Unlock
        // EndFrame, render distortion
        // Lock
        //
        // ...then the Lock would stall the CPU. So instead we defer the
        // re-Lock until the next time EndFrame is called, i.e.
        //
        // Lock(previously unlocked textures)
        // Unlock(used textures)
        // EndFrame, render distortion
        //
        // This seems complex, but it deals with the case where the app might
        // do EndFrame with the same texture multiple times without re-rendering
        // to it. It also removes any assumption about what order the app renders to
        // the textures in a swap chain in (i.e. incrementing or decrementing) -
        // the only assumption/requirement is that the app doesn't render to a
        // texture index it just submitted, until it's done EndFrame
        // with something else first.
        // 
        // In other words, it lets us have this code sequence...
        //
        // Lock(previously unlocked textures)
        // Unlock(used textures)
        // EndFrame, render distortion
        //
        // if ( random condition )
        // {
        //      set->CurrIndex = (set->CurrIndex + 1) % set->TextureCount;
        //      Render to set->Texture[CurrIndex];
        // }
        //
        // Lock(previously unlocked textures)
        // Unlock(used textures)
        // EndFrame, render distortion
        //
        // ...and whether or not the app decided to do the rendering
        // and advance the index, it all still works.
        //
        // Also note if we did something like detecting if CurrentIndex 
        // changed, that breaks if the app stops submitting the same texture set
        // and switches to another. Now one of the previous set's members 
        // will still be and if the app then tries to render to it, everything will die.
        // So instead we store a list of actual GL textures that have been unlocked
        // in UnlockedGlTextures and relock them next EndFrame.
        //
        // Also for efficiency, we'll first Unlock the new ones (ignoring and
        // removing any that are already unlocked), then Lock any remaining.
        //
        // VERY IMPORTANT THING. This assumes the state in CompositorLayers
        // is complete and canonical. That is, there's no implicit state pending from
        // previous frames on the server side (otherwise we'll Lock a texture that is
        // still going to be drawn on the screen). We used to allow sparse data on the 
        // client side, but that will break everything, so not any more.


        // Make a list of textures that might need to be relocked.
        std::vector<std::shared_ptr<GLTextureInterop>> ToBeLockedGlTextures;
        ToBeLockedGlTextures.reserve ( UnlockedGlTextures.size() );
        for ( auto glTex : UnlockedGlTextures )
        {
            OVR_ASSERT ( !glTex->Locked );
            ToBeLockedGlTextures.push_back ( glTex );
        }
        UnlockedGlTextures.clear();

        // ...and now Unlock any new ones, and remove any that are still in use.
        OVR_ASSERT ( CompositorLayers.GetSizeI() == MaxNumLayersPublic );
        for ( int layerNum = 0; layerNum < MaxNumLayersPublic; layerNum++ )
        {
            Service::CompositorLayerDesc *layer = &(CompositorLayers[layerNum]);
            uint32_t setIndex[4];
            setIndex[0] = layer->TextureIndexColor[0];
            setIndex[1] = layer->TextureIndexColor[0];
            setIndex[2] = layer->TextureIndexDepth[1];
            setIndex[3] = layer->TextureIndexDepth[0];

            // We already checked CompositorLayers and CompositorTextureSets were in agreement above.
            for ( int setNum = 0; setNum < 4; setNum++ )
            {
                TextureSet *set = CompositorTextureSets[layerNum][setNum];
                if ( set != nullptr )
                {
                    auto index = set->AppInfo.CurrentIndex;
                    OVR_ASSERT ( index < (int)set->GL_Textures.size() );
                    std::shared_ptr<GLTextureInterop> glTex = set->GL_Textures[index];
                    // The app wants to send this glTex to be rendered, so we need to Unlock it
                    // if it's not already unlocked.
                    if ( glTex->Locked )
                    {
                        // It's locked, so unlock it and add it to the list.
                        glTex->Unlock();
                        UnlockedGlTextures.push_back ( glTex );
                    }
                    else
                    {
                        // Already unlocked, so it should either be in the list from last frame,
                        // or we already added+unlocked it this frame.
                        auto it = std::find ( ToBeLockedGlTextures.begin(), ToBeLockedGlTextures.end(), glTex );
                        if ( it != ToBeLockedGlTextures.end() )
                        {
                            // We want to keep it unlocked.
                            ToBeLockedGlTextures.erase ( it );
                            UnlockedGlTextures.push_back ( glTex );
                        }
                        else
                        {
#ifdef OVR_BUILD_DEBUG
                            auto it = std::find ( UnlockedGlTextures.begin(), UnlockedGlTextures.end(), glTex );
                            OVR_ASSERT ( it != UnlockedGlTextures.end() );
#endif
                        }
                    }
                }
            }

            // And then all the textures that were used last frame and are NOT used this frame, re-lock them.
            for ( auto glTex : ToBeLockedGlTextures )
            {
                OVR_ASSERT ( !glTex->Locked );
                glTex->Lock();
            }
            ToBeLockedGlTextures.clear();
        }

    }


    OVRError err = compSubmitLayers(CompositorLayers);

    // TODO: Clean this code path up so we can properly report errors if this fails,
    // but still keep synchronization objects from seizing up
    if (!err.Succeeded())
    {
        return err;
    }

    // Without the flush rendering commands will be queued into the command buffer and not executed until full.
    // This renders very old geometry with unmatched poses and appears as several frames of latency.
    // Note this just gets them into the pending queue - it doesn't guarantee the GPU has actually started
    // the work (nor do we want to wait for that).
    D3D11_Context->Flush();

    HRESULT hr = S_OK;

    // Insert signal of fence at the end of current work
    if (Fence)
    {
        hr = Fence->ReleaseSync(0);
        OVR_HR_CHECK_RET_ERROR(ovrError_Timeout, hr, "Fence ReleaseSync");
    }
    else
    {
        // Slow Path, aka CPU spin wait
        D3D11_QUERY_DESC queryDesc = {};
        queryDesc.Query = D3D11_QUERY_EVENT;
        Ptr<ID3D11Query> query;

        OVR_D3D_CREATE(query, D3D11_Device->CreateQuery(&queryDesc, &query.GetRawRef()));

        D3D11_Context->End(query);
        BOOL done = FALSE;
        do {} while (!done && SUCCEEDED(D3D11_Context->GetData(query, &done, sizeof(done), 0)));
    }

    // Submit data to compositor
    err = compEndFrame(appFrameIndex, viewScaleDesc);
    if (!err.Succeeded())
    {
        return err;
    }

    // Wait for space in the present queue
    if (WaitForSingleObject(FrameQueueSemaphore.Get(), SynchronizationTimeoutMs) != WAIT_OBJECT_0)
    {
        // TODO: We probably don't want to error out in this case, but allow
        // retrying? Not sure, needs more thinking... multi-app focus, GPU throttling, etc...
        return OVR_MAKE_SYS_ERROR(ovrError_Timeout, ::GetLastError(), "Semaphore WaitForSingleObject");
    }

    // Insert wait on fence in context before returning to app to make
    // sure app's rendering is serailized behind compositor
    if (Fence)
    {
        hr = Fence->AcquireSync(0, SynchronizationTimeoutMs);
        OVR_HR_CHECK_RET_ERROR(ovrError_Timeout, hr, "Fence AcquireSync");
    }

    if (QueueAheadSeconds > 0.f)
    {
        // Compute nextFrameStartTime from next vsync (assumes we have a max queue ahead of 1),
        // working backwards the queue ahead amount.
        double nextFrameStartTime = HmdState->RenderTimer.GetNextVsyncTime() - QueueAheadSeconds;

        // If the queue ahead start time is in the past, don't just unblock immediately. That would
        // cause is to get stuck in "catch up" mode. Skip a frame to get back in sync.
        if (ovr_GetTimeInSeconds() > nextFrameStartTime)
        {
            nextFrameStartTime += HmdState->RenderTimer.GetFrameInterval();
        }

        while (ovr_GetTimeInSeconds() < nextFrameStartTime)
        {
            // Spin, SPIN, SPINNNNN!!!!
            // TODO: Make this something better than a spin wait :) ie. SetWaitableTimer
            // Spin waits risk using up most of your useful scheduler quantum here and will
            // lead to inopportune context switches during the game's actually useful code :(
        }

        TraceWaypoint(appFrameIndex);
    }
    TraceReturn(appFrameIndex);

    return OVRError::Success();
}

OVRError CliD3D11CompositorClient::SetQueueAheadSeconds(float queueAheadSeconds)
{
    if (queueAheadSeconds < 0 || queueAheadSeconds > HmdState->RenderTimer.GetFrameInterval())
    {
        return OVR_MAKE_ERROR(ovrError_InvalidParameter, "Invalid queue ahead amount specified.");
    }

    // If we've already created the sempahore, and this is a change in the current value,
    // then handle the cases where we enable/disable queue ahead
    if (QueueAheadSeconds != queueAheadSeconds && FrameQueueSemaphore.IsValid())
    {
        if (queueAheadSeconds == 0.f)
        {
            // Disabling queue ahead, eat up one of our semaphore counts to avoid queuing.
            if (WaitForSingleObject(FrameQueueSemaphore.Get(), SynchronizationTimeoutMs) != WAIT_OBJECT_0)
            {
                return OVR_MAKE_ERROR(ovrError_Timeout, "Failed to disable queue ahead.");
            }
        }
        else if (QueueAheadSeconds == 0.f)
        {
            // Enabling queue ahead, release the extra semaphore count to enable queueing.
            ReleaseSemaphore(FrameQueueSemaphore.Get(), 1, nullptr);
        }
    }

    QueueAheadSeconds = queueAheadSeconds;

    return OVRError::Success();
}

OVRError CliD3D11CompositorClient::initialize(ID3D11Device* device)
{
    ++DeviceReferences;

    if (Initialized)
    {
        // Already initialized
        return OVRError::Success();
    }

    HRESULT hr = S_OK;

    ClientRenderAPI = device ? ovrRenderAPI_D3D11 : ovrRenderAPI_OpenGL;

    if (ClientRenderAPI == ovrRenderAPI_D3D11)
    {
        D3D11_Device = device;
        device->GetImmediateContext(&D3D11_Context.GetRawRef());
    }
    else if (ClientRenderAPI == ovrRenderAPI_OpenGL)
    {
        GL::InitGLExtensions();

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        UINT flags = 0;
#ifdef OVR_BUILD_DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        OVR_ASSERT(!D3D11_Device && !D3D11_Context);
        D3D11_Device = nullptr;
        D3D11_Context = nullptr;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE,
                               nullptr, flags, &featureLevel, 1,
                               D3D11_SDK_VERSION, &D3D11_Device.GetRawRef(),
                               nullptr, &D3D11_Context.GetRawRef());
        if (FAILED(hr))
        {
            // If the failure was for a debug device,
            if ((flags & D3D11_CREATE_DEVICE_DEBUG) != 0)
            {
                // This can happen if a debug mode executable is being run on a system that does
                // not have the Windows SDK Layers DLL installed.  Windows Kit 8 includes it:
                // http://www.microsoft.com/en-us/download/details.aspx?id=42273
                return OVR_MAKE_SYS_ERROR_F(ovrError_DisplayInit, hr, "D3D11CreateDevice(debug) failed. Verify that you have Windows Kit 8 or newer installed.");
            }

            return OVR_MAKE_SYS_ERROR_F(ovrError_DisplayInit, hr, "D3D11CreateDevice");
        }

        OVR_D3D_TAG_OBJECT(D3D11_Device);
        OVR_D3D_TAG_OBJECT(D3D11_Context);

        GL_hDevice = wglDXOpenDeviceNV(D3D11_Device.GetPtr());
        if (!GL_hDevice)
        {
            return OVR_MAKE_SYS_ERROR(ovrError_Initialize, glGetError(), "wglDXOpenDeviceNV");
        }
    }

    Ptr<IDXGIDevice> dxgiDevice;
    hr = D3D11_Device->QueryInterface(IID_PPV_ARGS(&dxgiDevice.GetRawRef()));
    OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "QueryInterface device");

    Ptr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter.GetRawRef());
    OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "Device GetAdapter");

    DXGI_ADAPTER_DESC desc = {};
    hr = adapter->GetDesc(&desc);
    OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "Adapter GetDesc");

    // Pre-D3D12, the only way to get real fence objects via DXGI is through a keyedMutex.
    // Since we always do our fences on submission boundaries, we don't need one per texture.
    // We just need a single fence object to share with the compositor. So, create a dummy buffer
    // with the keyed mutex flag to get the sync object made, and then we'll just manually use
    // the keyed mutex object directly (without using the buffer).
    D3D11_BUFFER_DESC bd = {};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.StructureByteStride = 16;
    bd.ByteWidth = 16;
    bd.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    bd.Usage = D3D11_USAGE_DEFAULT;

    HANDLE fenceHandle = nullptr;

    // Pre-DXGI1.1 legacy support for pre-Unity 5 means that
    // we can't use the fences on those setups, so we CPU spin instead.
    // Therefore, this call can fail, but that's okay we just go forward
    // without the keyed mutex.
    Ptr<ID3D11Buffer> buffer;
    hr = D3D11_Device->CreateBuffer(&bd, nullptr, &buffer.GetRawRef());
    if (SUCCEEDED(hr))
    {
        OVR_D3D_TAG_OBJECT(buffer);
        hr = buffer->QueryInterface(IID_PPV_ARGS(&Fence.GetRawRef()));
        OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "QueryInterface buffer fence");

        Ptr<IDXGIResource> resource;
        hr = buffer->QueryInterface(IID_PPV_ARGS(&resource.GetRawRef()));
        OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "QueryInterface buffer resource");

        hr = resource->GetSharedHandle(&fenceHandle);
        OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "GetSharedHandle fence");

        // Application starts by owning the keyed mutex (fence object)
        hr = Fence->AcquireSync(0, SynchronizationTimeoutMs);
        OVR_HR_CHECK_RET_ERROR(ovrError_Initialize, hr, "Fence AcquireSync");
    }

    // Queue ahead is always enabled at the lowest layer, but we default QueueAheadFraction to 0
    const int PresentQueueLimit = 2;

    // Start initial count at one less than queue depth, since app starts off with 1 frame in-progress
    FrameQueueSemaphore.Attach(CreateSemaphore(nullptr, PresentQueueLimit - 1, PresentQueueLimit, nullptr));
    if (!FrameQueueSemaphore.IsValid())
    {
        return OVR_MAKE_SYS_ERROR(ovrError_Initialize, ::GetLastError(), "CreateSemaphore");
    }

    if (QueueAheadSeconds == 0.f)
    {
        // Acquire 1 count on the semaphore to prevent queue ahead
        if (WaitForSingleObject(FrameQueueSemaphore.Get(), SynchronizationTimeoutMs) != WAIT_OBJECT_0)
        {
            return OVR_MAKE_ERROR(ovrError_Initialize, "Failed to disable queue ahead.");
        }
    }

    // Get server process id
    pid_t serverProcessId = HmdState->pClient->GetServerProcessId();

    // Open server process to duplicate handle
    ScopedProcessHANDLE serverProcess(OpenProcess(PROCESS_DUP_HANDLE, FALSE, serverProcessId));
    if (!serverProcess.IsValid())
    {
        return OVR_MAKE_SYS_ERROR(ovrError_Initialize, ::GetLastError(), "OpenProcess");
    }

    // Duplicate handle for server
    // Note this is done here since the server is run as a normal user but
    // the game may have administrator level access.
    HANDLE serverFrameQueueSemaphore = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), FrameQueueSemaphore.Get(),
                         serverProcess.Get(), &serverFrameQueueSemaphore,
                         SYNCHRONIZE, FALSE, DUPLICATE_SAME_ACCESS))
    {
        return OVR_MAKE_SYS_ERROR(ovrError_Initialize, ::GetLastError(), "DuplicateHandle");
    }

    OVRError err = compConnect(desc.AdapterLuid, fenceHandle, serverFrameQueueSemaphore);
    if (!err.Succeeded())
    {
        return err;
    }

    Initialized = true;

    return OVRError::Success();
}

OVRError CliD3D11CompositorClient::uninitialize()
{
    OVRError err = OVRError::Success();

    --DeviceReferences;

    if (!DeviceReferences)
    {
        err = compDisconnect();
        if (!err.Succeeded())
            return err;

        FrameQueueSemaphore = nullptr;
        Fence = nullptr;
        D3D11_Context = nullptr;
        D3D11_Device = nullptr;

        Initialized = false;
    }

    return err;
}

OVRError CliD3D11CompositorClient::createTextureSetInternal(const D3D11_TEXTURE2D_DESC* desc,
                                                            std::shared_ptr<TextureSet>& textureSet,
                                                            std::vector<Ptr<ID3D11Texture2D>>& textures)
{
    D3D11_TEXTURE2D_DESC td = *desc;

    // Ensure the surface is configured for basic sharing
    td.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
    td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    if ( desc->SampleDesc.Count > 1 )
    {
        return OVR_MAKE_SYS_ERROR(ovrError_Initialize, ::GetLastError(), "MSAA SwapTextureSets not supported.");
    }
    if ( desc->ArraySize > 1 )
    {
        return OVR_MAKE_SYS_ERROR(ovrError_Initialize, ::GetLastError(), "Array SwapTextureSets not supported.");
    }

    // Validate and/or convert the format
    if ((td.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0)
    {
        // Add formats as and when we actually test them.
        switch ( td.Format )
        {
        // We can't actually create "real" depth/stencil formats, so alias them into the matching RGB equivalents.
        case DXGI_FORMAT_D32_FLOAT:         td.Format = DXGI_FORMAT_R32_TYPELESS;   break;
        case DXGI_FORMAT_D24_UNORM_S8_UINT: td.Format = DXGI_FORMAT_R24G8_TYPELESS; break;

        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_R24G8_TYPELESS:
            // Works.
            break;

        default:
            // Reject formats we know DON'T work with a useful error message.
            // Note - unlike the non-depth formats below, there's no "untested but might work" formats
            // because we need to write specific code in the compositor to reinterpret them.
            return OVR_MAKE_SYS_ERROR(ovrError_Initialize, ::GetLastError(), "Unsupported depth/stencil texture format.");
            break;
        }

        // Mutually exclusive with D3D11_BIND_DEPTH_STENCIL
        OVR_ASSERT ( ( td.BindFlags & D3D11_BIND_RENDER_TARGET ) == 0 );
    }
    else
    {
        // Add formats as and when we actually test them.
        switch ( td.Format )
        {
        // FIXME: Unity currently requests TYPELESS, but that's not technically supported.
        // Since this is the auto-blt path, we currently munge it to UNORM so it works.
        // Ideally, when we have the right API on top, we should disallow TYPELESS.
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:  td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:  td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; break;

        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            // We know these work.
            break;

        // These formats may or may not work. But probably do.
        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_B4G4R4A4_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
        case DXGI_FORMAT_B5G6R5_UNORM:
        //case DXGI_FORMAT_B8G8R8A8_UNORM:          // tested!
        //case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:     // tested!
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        //case DXGI_FORMAT_R8G8B8A8_UNORM:          // tested!
        //case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:     // tested!
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
            {
                // These stand a chance of working, but have not been tested.
                // TODO: add debug spew to the calling app about formats that we have never tried - might or might not work.
                static bool assertedOnUntestedformat = false;
                OVR_ASSERT ( assertedOnUntestedformat );
                assertedOnUntestedformat = true;
            }

            break;

        default:
            // Reject formats we know DON'T work with a useful error message.
            // Note - unlike the non-depth formats below, there's no "untested but might work" formats
            // because we need to write specific code in the compositor to reinterpret them.
            return OVR_MAKE_SYS_ERROR(ovrError_Initialize, ::GetLastError(), "Unsupported depth/stencil texture format.");
            break;
        }
    }

    // TODO: This number should not need to be changed for synchronous timewarp, and should
    // probably be derived from the present queue length for ATW cases. Either way, it is never
    // provided by the application as it's a function of synchronization between processes.
    static const int ChainDepth = 2;

    std::vector<HANDLE> shareHandles;

    textures.resize(ChainDepth);
    shareHandles.resize(ChainDepth);

    for (int i = 0; i < ChainDepth; ++i)
    {
        OVR_D3D_CREATE(textures[i], D3D11_Device->CreateTexture2D(&td, nullptr, &textures[i].GetRawRef()));

        Ptr<IDXGIResource> resource;
        HRESULT hr = textures[i]->QueryInterface(IID_PPV_ARGS(&resource.GetRawRef()));
        OVR_HR_CHECK_RET_ERROR_F(ovrError_Initialize, hr, "Chain %d QueryInterface", i);

        hr = resource->GetSharedHandle(&shareHandles[i]);
        OVR_HR_CHECK_RET_ERROR_F(ovrError_Initialize, hr, "Chain %d GetSharedHandle", i);
    }

    return compCreateTextureSet(shareHandles, &textureSet->ID);
}

CompositorLayerDesc* CliD3D11CompositorClient::findOrCreateLayerDesc(int layerNum)
{
    // This used to be a lot more exciting...
    OVR_ASSERT ( CompositorLayers.GetSizeI() == MaxNumLayersPublic );
    OVR_ASSERT ( ( layerNum >= 0 ) && ( layerNum < MaxNumLayersPublic ) );
    return &(CompositorLayers[layerNum]);
}


}} // namespace OVR::CAPI

#endif // OVR_OS_WIN32
