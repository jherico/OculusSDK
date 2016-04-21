/************************************************************************************
Content     :   First-person view test application for Oculus Rift
Created     :   19th June 2015
Authors     :   Tom Heath
Copyright   :   Copyright 2015 Oculus, Inc. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/
/// Renders tracked triangles on the Rift, no mirror, everything in one file.
/// This sample is very good for tracking critical changes in the SDK
/// by providing a minimal diff.  Halts automatically after a short time.

#include "d3d11.h"
#include "d3dcompiler.h"
#include "OVR_CAPI_D3D.h"
#include "DirectXMath.h"
using namespace DirectX;
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
void CreateSampleModel(ID3D11Device * Device, ID3D11DeviceContext * Context);
void RenderSampleModel(XMMATRIX * viewProj, ID3D11Device * Device, ID3D11DeviceContext * Context);

//-------------------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
	// Init Rift and device
	ovr_Initialize(0);  ovrGraphicsLuid luid;
	ovrSession session;	ovr_Create(&session, &luid);
	ID3D11DeviceContext * Context;
	IDXGIFactory * DXGIFactory; CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)(&DXGIFactory));
	IDXGIAdapter * DXGIAdapter;  DXGIFactory->EnumAdapters(0, &DXGIAdapter);
	ID3D11Device * Device;       D3D11CreateDevice(DXGIAdapter, D3D_DRIVER_TYPE_UNKNOWN, 0, 0, 0, 0, D3D11_SDK_VERSION, &Device, 0, &Context);

	// Create eye render buffers
	ID3D11RenderTargetView * eyeRenderTexRtv[2][10];
	ovrLayerEyeFov ld = { { ovrLayerType_EyeFov } };
	for (int i = 0; i < 2; i++)
	{
		ld.Fov[i] = ovr_GetHmdDesc(session).DefaultEyeFov[i];
		ld.Viewport[i].Size = ovr_GetFovTextureSize(session, (ovrEyeType)i, ld.Fov[i], 1.0f);
		ovrTextureSwapChainDesc dsDesc = { ovrTexture_2D, OVR_FORMAT_R8G8B8A8_UNORM_SRGB, 1, ld.Viewport[i].Size.w, ld.Viewport[i].Size.h,
			1, 1, ovrFalse, ovrTextureMisc_DX_Typeless, ovrTextureBind_DX_RenderTarget };
		ovr_CreateTextureSwapChainDX(session, Device, &dsDesc, &ld.ColorTexture[i]);
		int textureCount = 0; ovr_GetTextureSwapChainLength(session, ld.ColorTexture[i], &textureCount);
		for (int j = 0; j < textureCount; j++)
		{
			ID3D11Texture2D* tex; ovr_GetTextureSwapChainBufferDX(session, ld.ColorTexture[i], j, IID_PPV_ARGS(&tex));
			D3D11_RENDER_TARGET_VIEW_DESC rtvd = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_RTV_DIMENSION_TEXTURE2D };
			Device->CreateRenderTargetView(tex, &rtvd, &eyeRenderTexRtv[i][j]);
		}
	}

	// Create sample model to be rendered in VR
	CreateSampleModel(Device,Context);

	// Loop for some frames, then terminate
	for (long long frameIndex = 0; frameIndex < 1000;)
	{
		// Get pose using a default IPD
		ovrVector3f HmdToEyeOffset[2] = { { -0.032f, 0, 0 }, { +0.032f, 0, 0 }, };
        ovrPosef pose[2]; ovr_GetEyePoses(session, frameIndex, ovrTrue, HmdToEyeOffset, pose, &ld.SensorSampleTime);
		for (int i = 0; i < 2; i++) ld.RenderPose[i] = pose[i];

		// Render to each eye
		for (int i = 0; i < 2; i++)
		{
			// Set and clear current render target, and set viewport
			int index = 0;	ovr_GetTextureSwapChainCurrentIndex(session, ld.ColorTexture[i], &index);
			Context->OMSetRenderTargets(1, &eyeRenderTexRtv[i][index], 0);
			Context->ClearRenderTargetView(eyeRenderTexRtv[i][index], new float[4]);
			D3D11_VIEWPORT D3Dvp = { 0, 0, (float)ld.Viewport[i].Size.w, (float)ld.Viewport[i].Size.h };
			Context->RSSetViewports(1, &D3Dvp);

			// Calculate view and projection matrices using pose and SDK
			XMVECTOR rot = XMLoadFloat4((XMFLOAT4 *)&pose[i].Orientation);
			XMVECTOR pos = XMLoadFloat3((XMFLOAT3 *)&pose[i].Position);
			XMVECTOR up = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rot);
			XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, -1, 0), rot);
			XMMATRIX view = XMMatrixLookAtRH(pos, XMVectorAdd(pos, forward), up);
            ovrMatrix4f p = ovrMatrix4f_Projection(ld.Fov[i], 0, 10, ovrProjection_None);
		    XMMATRIX proj = XMMatrixTranspose(XMLoadFloat4x4((XMFLOAT4X4 *)&p)); 

			// Render model and commit frame
			RenderSampleModel(&XMMatrixMultiply(view, proj), Device, Context);
			ovr_CommitTextureSwapChain(session, ld.ColorTexture[i]);
		}

		// Send rendered eye buffers to HMD, and increment the frame if we're visible
		ovrLayerHeader* layers[1] = { &ld.Header };
		if (ovrSuccess == ovr_SubmitFrame(session, frameIndex, nullptr, layers, 1)) frameIndex++;
	}

	ovr_Shutdown();
}




//---------------------------------------------------------------------------------------
// THIS CODE IS NOT SPECIFIC TO VR OR THE SDK, JUST USED TO DRAW SOMETHING IN VR
//---------------------------------------------------------------------------------------
void CreateSampleModel(ID3D11Device * Device, ID3D11DeviceContext * Context)
{
    #define V(n) (n&1?+1.0f:-1.0f), (n&2?-1.0f:+1.0f), (n&4?+1.0f:-1.0f) 
	float vertices[] = { V(0), V(3), V(2), V(6), V(3), V(7), V(4), V(2), V(6), V(1), V(5), V(3), V(4), V(1), V(0), V(5), V(4), V(7) };
	D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
	D3D11_SUBRESOURCE_DATA initData = { vertices };
	ID3D11Buffer* VertexBuffer;  Device->CreateBuffer(&bd, &initData, &VertexBuffer);
	UINT stride = sizeof(float)* 3U;	UINT offset = 0;
	Context->IASetVertexBuffers(0, 1, &VertexBuffer, &stride, &offset);

	char* vShader = "float4x4 m; void VS( in float4 p1 : POSITION, out float4 p2 : SV_Position ) { p2 = mul(m, p1); }";
	ID3D10Blob * pBlob; D3DCompile(vShader, strlen(vShader), "VS", 0, 0, "VS", "vs_4_0", 0, 0, &pBlob, 0);
	ID3D11VertexShader * VertexShader; Device->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), 0, &VertexShader);
	D3D11_INPUT_ELEMENT_DESC elements[] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT }, };
	ID3D11InputLayout  * InputLayout; Device->CreateInputLayout(elements, 1, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &InputLayout);
	char* pShader = "void PS(out float4 colorOut : SV_Target) { colorOut = float4(0.1,0.5,0.1,1); }";
	D3DCompile(pShader, strlen(pShader), "PS", 0, 0, "PS", "ps_4_0", 0, 0, &pBlob, 0);
	ID3D11PixelShader  * PixelShader; Device->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), 0, &PixelShader);
	Context->IASetInputLayout(InputLayout);
	Context->VSSetShader(VertexShader, 0, 0);
	Context->PSSetShader(PixelShader, 0, 0);
}
//------------------------------------------------------
void RenderSampleModel(XMMATRIX * viewProj, ID3D11Device * Device, ID3D11DeviceContext * Context)
{
	D3D11_BUFFER_DESC desc = { sizeof(XMMATRIX), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE };
	D3D11_SUBRESOURCE_DATA initData = { viewProj };
	ID3D11Buffer * ConstantBuffer;  Device->CreateBuffer(&desc, &initData, &ConstantBuffer); 
	Context->VSSetConstantBuffers(0, 1, &ConstantBuffer);
	Context->Draw(24, 0);
}

