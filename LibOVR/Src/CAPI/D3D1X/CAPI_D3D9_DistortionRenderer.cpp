/************************************************************************************

Filename    :   CAPI_D3D1X_DistortionRenderer.cpp
Content     :   Experimental distortion renderer
Created     :   March 7th, 2014
Authors     :   Tom Heath

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.1 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.1 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "CAPI_D3D9_DistortionRenderer.h"
#define OVR_D3D_VERSION 9
#include "../../OVR_CAPI_D3D.h"

namespace OVR { namespace CAPI { namespace D3D9 {


///QUESTION : Why not just a normal constructor?
CAPI::DistortionRenderer* DistortionRenderer::Create(ovrHmd hmd,
                                                     FrameTimeManager& timeManager,
                                                     const HMDRenderState& renderState)
{
    return new DistortionRenderer(hmd, timeManager, renderState);
}

DistortionRenderer::DistortionRenderer(ovrHmd hmd, FrameTimeManager& timeManager,
                                       const HMDRenderState& renderState)
    : CAPI::DistortionRenderer(ovrRenderAPI_D3D9, hmd, timeManager, renderState) 
{
}
/**********************************************/
DistortionRenderer::~DistortionRenderer()
{
	//Release any memory 
	eachEye[0].dxIndices->Release();
	eachEye[0].dxVerts->Release();
	eachEye[1].dxIndices->Release();
	eachEye[1].dxVerts->Release();
}


/******************************************************************************/
bool DistortionRenderer::Initialize(const ovrRenderAPIConfig* apiConfig,
                                    unsigned hmdCaps, unsigned arg_distortionCaps)
{
	// TBD: Decide if hmdCaps are needed here or are a part of RenderState
    OVR_UNUSED(hmdCaps);

	///QUESTION - what is returned bool for???  Are we happy with this true, if not config.
    const ovrD3D9Config * config = (const ovrD3D9Config*)apiConfig;
    if (!config)                return true; 
    if (!config->D3D9.pDevice)  return false;

	//Glean all the required variables from the input structures
	device         = config->D3D9.pDevice;
	screenSize     = config->D3D9.Header.RTSize;
	distortionCaps = arg_distortionCaps;

	CreateVertexDeclaration();
	CreateDistortionShaders();
	Create_Distortion_Models();

    return (true);
}


/**************************************************************/
void DistortionRenderer::SubmitEye(int eyeId, ovrTexture* eyeTexture)
{
	//Doesn't do a lot in here??
	const ovrD3D9Texture* tex = (const ovrD3D9Texture*)eyeTexture;

	//Write in values
    eachEye[eyeId].texture = tex->D3D9.pTexture;

	//Its only at this point we discover what the viewport of the texture is.
	//because presumably we allow users to realtime adjust the resolution.
	//Which begs the question - why did we ask them what viewport they were
	//using before, which gave them a set of UV offsets.   In fact, our 
	//asking for eye mesh must be entirely independed of these viewports,
	//presumably only to get the parameters.

    ovrEyeDesc     ed = RState.EyeRenderDesc[eyeId].Desc;
    ed.TextureSize    = tex->D3D9.Header.TextureSize;
    ed.RenderViewport = tex->D3D9.Header.RenderViewport;

     ovrHmd_GetRenderScaleAndOffset(HMD, ed, distortionCaps, eachEye[eyeId].UVScaleOffset);
}


/******************************************************************/
void DistortionRenderer::EndFrame(bool swapBuffers, unsigned char* latencyTesterDrawColor, unsigned char* latencyTester2DrawColor)
{
	OVR_UNUSED(swapBuffers);
	OVR_UNUSED(latencyTesterDrawColor);

	///QUESTION : Should I be clearing the screen? 
	///QUESTION : Should I be ensuring the screen is the render target

	if (!TimeManager.NeedDistortionTimeMeasurement())
    {
		if (RState.DistortionCaps & ovrDistortion_TimeWarp)
		{
			// Wait for timewarp distortion if it is time and Gpu idle
			WaitTillTimeAndFlushGpu(TimeManager.GetFrameTiming().TimewarpPointTime);
		}

        RenderBothDistortionMeshes();
    }
    else
    {
        // If needed, measure distortion time so that TimeManager can better estimate
        // latency-reducing time-warp wait timing.
        WaitUntilGpuIdle();
        double  distortionStartTime = ovr_GetTimeInSeconds();

        RenderBothDistortionMeshes();
        WaitUntilGpuIdle();

        TimeManager.AddDistortionTimeMeasurement(ovr_GetTimeInSeconds() - distortionStartTime);
    }

    if(latencyTesterDrawColor)
    {
		///QUESTION : Is this still to be supported?
        ///renderLatencyQuad(latencyTesterDrawColor);
    }

    if(latencyTester2DrawColor)
    {
        // TODO:
    }

    if (swapBuffers)
    {
		device->Present( NULL, NULL, NULL, NULL );

    ///    if (RParams.pSwapChain)
        {
     ///       UINT swapInterval = (RState.HMDCaps & ovrHmdCap_NoVSync) ? 0 : 1;
     ///       RParams.pSwapChain->Present(swapInterval, 0);

            // Force GPU to flush the scene, resulting in the lowest possible latency.
            // It's critical that this flush is *after* present.
     ///       WaitUntilGpuIdle();
        }
    ///    else
        {
            // TBD: Generate error - swapbuffer option used with null swapchain.
        }
    }
}


void DistortionRenderer::WaitUntilGpuIdle()
{
#if 0
    // Flush and Stall CPU while waiting for GPU to complete rendering all of the queued draw calls
    D3D1x_QUERY_DESC queryDesc = { D3D1X_(QUERY_EVENT), 0 };
    Ptr<ID3D1xQuery> query;
    BOOL             done = FALSE;

    if (RParams.pDevice->CreateQuery(&queryDesc, &query.GetRawRef()) == S_OK)
    {
        D3DSELECT_10_11(query->End(),
                        RParams.pContext->End(query));

        // GetData will returns S_OK for both done == TRUE or FALSE.
        // Exit on failure to avoid infinite loop.
        do { }
        while(!done &&
              !FAILED(D3DSELECT_10_11(query->GetData(&done, sizeof(BOOL), 0),
                                      RParams.pContext->GetData(query, &done, sizeof(BOOL), 0)))
             );
    }
#endif
}

double DistortionRenderer::WaitTillTimeAndFlushGpu(double absTime)
{

OVR_UNUSED(absTime);
#if 0
	double       initialTime = ovr_GetTimeInSeconds();
	if (initialTime >= absTime)
		return 0.0;

	// Flush and Stall CPU while waiting for GPU to complete rendering all of the queued draw calls
    D3D1x_QUERY_DESC queryDesc = { D3D1X_(QUERY_EVENT), 0 };
    Ptr<ID3D1xQuery> query;
    BOOL             done = FALSE;
	bool             callGetData = false;

    if (RParams.pDevice->CreateQuery(&queryDesc, &query.GetRawRef()) == S_OK)
    {
        D3DSELECT_10_11(query->End(),
                        RParams.pContext->End(query));
		callGetData = true;
	}

	double newTime   = initialTime;
	volatile int i;

	while (newTime < absTime)
	{
		if (callGetData)
		{
			// GetData will returns S_OK for both done == TRUE or FALSE.
			// Stop calling GetData on failure.
			callGetData = !FAILED(D3DSELECT_10_11(query->GetData(&done, sizeof(BOOL), 0),
					                              RParams.pContext->GetData(query, &done, sizeof(BOOL), 0))) && !done;
		}
		else
		{
			for (int j = 0; j < 50; j++)
				i = 0;
		}
		newTime = ovr_GetTimeInSeconds();
	}

	// How long we waited
	return newTime - initialTime;
#endif
	return 0; //dummy
}







}}} // OVR::CAPI::D3D1X


