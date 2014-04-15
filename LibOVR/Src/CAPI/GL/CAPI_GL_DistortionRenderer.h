/************************************************************************************

Filename    :   CAPI_GL_DistortionRenderer.h
Content     :   Distortion renderer header for GL
Created     :   November 11, 2013
Authors     :   David Borel, Lee Cooper

Copyright   :   Copyright 2013 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus Inc license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

************************************************************************************/

#ifndef OVR_CAPI_GL_DistortionRenderer_h
#define OVR_CAPI_GL_DistortionRenderer_h

#include "../CAPI_DistortionRenderer.h"

#include "../../Kernel/OVR_Log.h"
#include "CAPI_GL_Util.h"

namespace OVR { namespace CAPI { namespace GL {

// ***** GL::DistortionRenderer

// Implementation of DistortionRenderer for GL.

class DistortionRenderer : public CAPI::DistortionRenderer
{
public:    
    DistortionRenderer(ovrHmd hmd,
                       FrameTimeManager& timeManager,
                       const HMDRenderState& renderState);
    ~DistortionRenderer();

    
    // Creation function for the device.    
    static CAPI::DistortionRenderer* Create(ovrHmd hmd,
                                            FrameTimeManager& timeManager,
                                            const HMDRenderState& renderState);


    // ***** Public DistortionRenderer interface
	
    virtual bool Initialize(const ovrRenderAPIConfig* apiConfig,
                            unsigned hmdCaps, unsigned distortionCaps);

    virtual void SubmitEye(int eyeId, ovrTexture* eyeTexture);

    virtual void EndFrame(bool swapBuffers, unsigned char* latencyTesterDrawColor, unsigned char* latencyTester2DrawColor);

    void         WaitUntilGpuIdle();

	// Similar to ovr_WaitTillTime but it also flushes GPU.
	// Note, it exits when time expires, even if GPU is not in idle state yet.
	double       FlushGpuAndWaitTillTime(double absTime);

private:    
    // TBD: Should we be using oe from RState instead?
    unsigned            DistortionCaps;

	struct FOR_EACH_EYE
	{
#if 0
		IDirect3DVertexBuffer9  * dxVerts;
		IDirect3DIndexBuffer9   * dxIndices;
#endif
		int                       numVerts;
		int                       numIndices;

		GLuint                     texture;

		ovrVector2f			 	  UVScaleOffset[2]; 
	} eachEye[2];

    // GL context and utility variables.
    RenderParams        RParams;    

	// Helpers
    void initBuffersAndShaders();
    void initShaders();
    void initFullscreenQuad();
    void destroy();
	
    void setViewport(const Recti& vp);

    void renderDistortion(Texture* leftEyeTexture, Texture* rightEyeTexture);

    void renderPrimitives(const ShaderFill* fill, Buffer* vertices, Buffer* indices,
                          Matrix4f* viewMatrix, int offset, int count,
                          PrimitiveType rprim, bool useDistortionVertex);

	void createDrawQuad();
    void renderLatencyQuad(unsigned char* latencyTesterDrawColor);
    void renderLatencyPixel(unsigned char* latencyTesterPixelColor);
	
    Ptr<Texture>        pEyeTextures[2];

    // U,V scale and offset needed for timewarp.
    ovrVector2f         UVScaleOffset[2][2];

	Ptr<Buffer>         DistortionMeshVBs[2];    // one per-eye
	Ptr<Buffer>         DistortionMeshIBs[2];    // one per-eye

	Ptr<ShaderSet>      DistortionShader;

    struct StandardUniformData
    {
        Matrix4f  Proj;
        Matrix4f  View;
    }                   StdUniforms;

    Ptr<Buffer>         LatencyTesterQuadVB;
    Ptr<ShaderSet>      SimpleQuadShader;

    Ptr<Texture>             CurRenderTarget;
    Array<Ptr<Texture> >     DepthBuffers;
    GLuint                   CurrentFbo;
};

}}} // OVR::CAPI::GL

#endif // OVR_CAPI_GL_DistortionRenderer_h