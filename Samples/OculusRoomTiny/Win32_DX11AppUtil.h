/************************************************************************************
Filename    :   Win32_DX11AppUtil.h
Content     :   D3D11 and Application/Window setup functionality for RoomTiny
Created     :   October 20th, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2014 Oculus, Inc. All Rights reserved.
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

#include <Extras/OVR_Math.h>
using namespace OVR;

#include <stddef.h>
#include <d3d11.h>
#include <D3D11Shader.h>
#include <d3dcompiler.h>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

//---------------------------------------------------------------------
struct DirectX11
{
    HWND                     Window;
    bool                     Key[256];
    OVR::Sizei               WinSize;
    struct DepthBuffer     * MainDepthBuffer;
    ID3D11Device           * Device;
    ID3D11DeviceContext    * Context;
    IDXGISwapChain         * SwapChain;
    ID3D11Texture2D        * BackBuffer;
    ID3D11RenderTargetView * BackBufferRT;
    struct DataBuffer      * UniformBufferGen;

    bool InitWindowAndDevice(HINSTANCE hinst, OVR::Recti vp, bool windowed, char *);
    void ClearAndSetRenderTarget(ID3D11RenderTargetView * rendertarget, struct DepthBuffer * depthbuffer, OVR::Recti vp);

	bool IsAnyKeyPressed() const
	{
		for (unsigned i = 0; i < (sizeof(Key) / sizeof(Key[0])); i++)
			if (Key[i]) return true;
		return false;
	}

    void SetMaxFrameLatency(int value)
	{
		IDXGIDevice1* DXGIDevice1 = NULL;
		HRESULT hr = Device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&DXGIDevice1);
		if (FAILED(hr) | (DXGIDevice1 == NULL)) return;
		DXGIDevice1->SetMaximumFrameLatency(value);
		DXGIDevice1->Release();
	}

 	void WaitUntilGpuIdle()
	{
		D3D11_QUERY_DESC queryDesc = { D3D11_QUERY_EVENT, 0 };
		ID3D11Query *  query;
		BOOL           done = FALSE;
		if (Device->CreateQuery(&queryDesc, &query) == S_OK)
		{
			Context->End(query);
			while (!done && !FAILED(Context->GetData(query, &done, sizeof(BOOL), 0)));
		}
	}

	void HandleMessages()
	{
		MSG msg;
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			switch (msg.message)
			{
				case WM_KEYDOWN: Key[msg.wParam] = true;  break;
				case WM_KEYUP:   Key[msg.wParam] = false; break;
			}
		}
	}

	void OutputFrameTime(double currentTime)
	{
		static double lastTime = 0;
		char tempString[100];
		sprintf_s(tempString, "Frame time = %0.2f ms\n", (currentTime - lastTime)*1000.0f);
		OutputDebugStringA(tempString);
		lastTime = currentTime;
	}

	void ReleaseWindow(HINSTANCE hinst)
	{
		ReleaseCapture(); 
		ShowCursor(TRUE);
		DestroyWindow(Window);
		UnregisterClassW(L"OVRAppWindow", hinst);
	}
} Platform;

//------------------------------------------------------------
struct DepthBuffer
{
    ID3D11DepthStencilView * TexDsv;

    DepthBuffer::DepthBuffer(Sizei size, int sampleCount)
    {
        DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT;

        // check for multisampling support
        UINT numQualityLevels;
        Platform.Device->CheckMultisampleQualityLevels(format, sampleCount, &numQualityLevels);
        if (numQualityLevels == 0) sampleCount = 1; // disable multisampling if not supported

        D3D11_TEXTURE2D_DESC dsDesc;
        dsDesc.Width = size.w;
        dsDesc.Height = size.h;
        dsDesc.MipLevels = 1;
        dsDesc.ArraySize = 1;
        dsDesc.Format = format;
        dsDesc.SampleDesc.Count = sampleCount;
        dsDesc.SampleDesc.Quality = 0;
        dsDesc.Usage = D3D11_USAGE_DEFAULT;
        dsDesc.CPUAccessFlags = 0;
        dsDesc.MiscFlags = 0;
        dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ID3D11Texture2D * Tex;
        Platform.Device->CreateTexture2D(&dsDesc, NULL, &Tex);
        Platform.Device->CreateDepthStencilView(Tex, NULL, &TexDsv);
    }
};

//----------------------------------------------------------------
struct DataBuffer
{
    ID3D11Buffer * D3DBuffer;
    size_t         Size;

    DataBuffer(D3D11_BIND_FLAG use, const void* buffer, size_t size) : Size(size)
    {
        D3D11_BUFFER_DESC desc;   memset(&desc, 0, sizeof(desc));
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.BindFlags = use;
        desc.ByteWidth = (unsigned)size;
        D3D11_SUBRESOURCE_DATA sr;
        sr.pSysMem = buffer;
        sr.SysMemPitch = sr.SysMemSlicePitch = 0;
        Platform.Device->CreateBuffer(&desc, buffer ? &sr : NULL, &D3DBuffer);
    }

    void Refresh(ID3D11DeviceContext * Context, const void* buffer, size_t size)
    {
        D3D11_MAPPED_SUBRESOURCE map;
        Context->Map(D3DBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
        memcpy((void *)map.pData, buffer, size);
        Context->Unmap(D3DBuffer, 0);
    }
};

//------------------------------------------------------------
struct TextureBuffer
{
    ID3D11Texture2D            * Tex;
    ID3D11ShaderResourceView   * TexSv;
    ID3D11RenderTargetView     * TexRtv;
    OVR::Sizei                   Size;

	TextureBuffer(bool rendertarget, Sizei size, int mipLevels,
		unsigned char * data, int sampleCount) : Size(size)
	{
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

		// check for multisampling support
		UINT numQualityLevels;
		Platform.Device->CheckMultisampleQualityLevels(format, sampleCount, &numQualityLevels);
		if (numQualityLevels == 0) sampleCount = 1; // disable multisampling if not supported

		D3D11_TEXTURE2D_DESC dsDesc;
		dsDesc.Width = size.w;
		dsDesc.Height = size.h;
		dsDesc.MipLevels = mipLevels;
		dsDesc.ArraySize = 1;
		dsDesc.Format = format;
		dsDesc.SampleDesc.Count = sampleCount;
		dsDesc.SampleDesc.Quality = 0;
		dsDesc.Usage = D3D11_USAGE_DEFAULT;
		dsDesc.CPUAccessFlags = 0;
		dsDesc.MiscFlags = 0;
		dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		if (rendertarget) dsDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		Platform.Device->CreateTexture2D(&dsDesc, NULL, &Tex);
		Platform.Device->CreateShaderResourceView(Tex, NULL, &TexSv);
		if (rendertarget) Platform.Device->CreateRenderTargetView(Tex, NULL, &TexRtv);

		if (data) // Note data is trashed, as is width and height
		{
			for (int level = 0; level < mipLevels; level++)
			{
				Platform.Context->UpdateSubresource(Tex, level, NULL, data, size.w * 4, size.h * 4);
				for (int j = 0; j < (size.h & ~1); j += 2)
				{
					const uint8_t* psrc = data + (size.w * j * 4);
					uint8_t*       pdest = data + ((size.w >> 1) * (j >> 1) * 4);
					for (int i = 0; i < size.w >> 1; i++, psrc += 8, pdest += 4)
					{
						pdest[0] = (((int)psrc[0]) + psrc[4] + psrc[size.w * 4 + 0] + psrc[size.w * 4 + 4]) >> 2;
						pdest[1] = (((int)psrc[1]) + psrc[5] + psrc[size.w * 4 + 1] + psrc[size.w * 4 + 5]) >> 2;
						pdest[2] = (((int)psrc[2]) + psrc[6] + psrc[size.w * 4 + 2] + psrc[size.w * 4 + 6]) >> 2;
						pdest[3] = (((int)psrc[3]) + psrc[7] + psrc[size.w * 4 + 3] + psrc[size.w * 4 + 7]) >> 2;
					}
				}
				size.w >>= 1;  size.h >>= 1;
			}
		}
	}
			
	OVR::Sizei GetSize() {   return Size;  }

	void SetAndClearRenderSurface(DepthBuffer * zbuffer) ///NOT DONE VIEWPORT
	{
		float black[] = { 0, 0, 0, 1 };
		Platform.Context->OMSetRenderTargets(1, &TexRtv, zbuffer->TexDsv);
		Platform.Context->ClearRenderTargetView(TexRtv, black);
		Platform.Context->ClearDepthStencilView(zbuffer->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
		D3D11_VIEWPORT D3Dvp;
		D3Dvp.Width = (float)Size.w;    D3Dvp.Height = (float)Size.h;
		D3Dvp.MinDepth = 0;              D3Dvp.MaxDepth = 1;
		D3Dvp.TopLeftX = 0;/*(float)vp.x;*/    D3Dvp.TopLeftY = 0;//(float)vp.y;
		Platform.Context->RSSetViewports(1, &D3Dvp);
	}

    // resolve MSAA texture
    void ResolveMSAA(TextureBuffer *dst)
    {
        Platform.Context->ResolveSubresource(dst->Tex, 0, Tex, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    }
};

//--------------------------------------------------------------------------
struct Shader 
{
    struct Uniform  { char Name[40];int Offset, Size; };
    ID3D11VertexShader * D3DVert;
    ID3D11PixelShader  * D3DPix;
    unsigned char      * UniformData;
    int                  UniformsSize;
	int                  numUniformInfo;
    Uniform              UniformInfo[10];
 
	Shader(ID3D10Blob* s, int which_type) :
        numUniformInfo(0),
        UniformData(nullptr)
	{
		if (which_type == 0) Platform.Device->CreateVertexShader(s->GetBufferPointer(), s->GetBufferSize(), NULL, &D3DVert);
		else                 Platform.Device->CreatePixelShader(s->GetBufferPointer(), s->GetBufferSize(), NULL, &D3DPix);

		ID3D11ShaderReflection* ref;
		D3DReflect(s->GetBufferPointer(), s->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&ref);
		ID3D11ShaderReflectionConstantBuffer* buf = ref->GetConstantBufferByIndex(0);
		D3D11_SHADER_BUFFER_DESC bufd;
		if (FAILED(buf->GetDesc(&bufd))) return;

		for (unsigned i = 0; i < bufd.Variables; i++)
		{
			ID3D11ShaderReflectionVariable* var = buf->GetVariableByIndex(i);
			D3D11_SHADER_VARIABLE_DESC vd;
			var->GetDesc(&vd);
			Uniform u;
			strcpy_s(u.Name, (const char*)vd.Name);
			u.Offset = vd.StartOffset;
			u.Size = vd.Size;
			UniformInfo[numUniformInfo++] = u;
		}
		UniformsSize = bufd.Size;
		UniformData = new unsigned char[bufd.Size];
	}
    ~Shader()
    {
        if (UniformData)
        {
        delete[] UniformData;
	}
    }

	void SetUniform(const char* name, int n, const float* v)
	{
		for (int i = 0; i<numUniformInfo; i++)
		{
			if (!strcmp(UniformInfo[i].Name, name))
			{
				memcpy(UniformData + UniformInfo[i].Offset, v, n * sizeof(float));
				break;
			}
		}
	}
};

//-----------------------------------------------------
struct ShaderFill
{
    Shader             * VShader, *PShader;
    TextureBuffer      * OneTexture;
    ID3D11InputLayout  * InputLayout;
	UINT                 VertexSize;
    ID3D11SamplerState * SamplerState;

    ShaderFill(D3D11_INPUT_ELEMENT_DESC * VertexDesc, int numVertexDesc,
               char* vertexShader, char* pixelShader, TextureBuffer * t, int vSize, bool wrap=1) : OneTexture(t), VertexSize(vSize)
	{
		ID3D10Blob *blobData;
		D3DCompile(vertexShader, strlen(vertexShader), NULL, NULL, NULL, "main", "vs_4_0", 0, 0, &blobData, NULL);
		VShader = new Shader(blobData, 0);

		Platform.Device->CreateInputLayout(VertexDesc, numVertexDesc,
			blobData->GetBufferPointer(), blobData->GetBufferSize(), &InputLayout);
		D3DCompile(pixelShader, strlen(pixelShader), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &blobData, NULL);
		PShader = new Shader(blobData, 1);

		D3D11_SAMPLER_DESC ss; memset(&ss, 0, sizeof(ss));
		ss.AddressU = ss.AddressV = ss.AddressW = wrap ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_BORDER;
		ss.Filter = D3D11_FILTER_ANISOTROPIC;
		ss.MaxAnisotropy = 8;
		ss.MaxLOD = 15;
		Platform.Device->CreateSamplerState(&ss, &SamplerState);
	}
};

//---------------------------------------------------------------------------
struct Model 
{
    struct Color
    { 
        unsigned char R,G,B,A;
        
        Color(unsigned char r = 0,unsigned char g=0,unsigned char b=0, unsigned char a = 0xff)
            : R(r), G(g), B(b), A(a) 
        { }
    };
    struct Vertex
    { 
        OVR::Vector3f  Pos;
        Color     C;
        float     U, V;
    };

    OVR::Vector3f     Pos;
    OVR::Quatf        Rot;
    OVR::Matrix4f     Mat;
    int          numVertices, numIndices;
    Vertex       Vertices[2000]; //Note fixed maximum
    uint16_t     Indices[2000];
    ShaderFill * Fill;
    DataBuffer * VertexBuffer, * IndexBuffer;  
   
    Model(OVR::Vector3f arg_pos, ShaderFill * arg_Fill) { Pos = arg_pos; Fill = arg_Fill; numVertices=numIndices=0; VertexBuffer=IndexBuffer=0;}
    OVR::Matrix4f& GetMatrix()                          { Mat = OVR::Matrix4f(Rot); Mat = OVR::Matrix4f::Translation(Pos) * Mat; return Mat; }
    void AddVertex(const Vertex& v)                     { Vertices[numVertices++] = v;  }
    void AddIndex(uint16_t a)                           { Indices[numIndices++] = a;    }
    void AllocateBuffers()
	{
		VertexBuffer = new DataBuffer(D3D11_BIND_VERTEX_BUFFER, &Vertices[0], numVertices * sizeof(Vertex));
		IndexBuffer  = new DataBuffer(D3D11_BIND_INDEX_BUFFER, &Indices[0], numIndices * 2);
	}

	void AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2, Color c)
	{
		Vector3f Vert[][2] =
		{ Vector3f(x1, y2, z1), Vector3f(z1, x1), Vector3f(x2, y2, z1), Vector3f(z1, x2),
		Vector3f(x2, y2, z2), Vector3f(z2, x2), Vector3f(x1, y2, z2), Vector3f(z2, x1),
		Vector3f(x1, y1, z1), Vector3f(z1, x1), Vector3f(x2, y1, z1), Vector3f(z1, x2),
		Vector3f(x2, y1, z2), Vector3f(z2, x2), Vector3f(x1, y1, z2), Vector3f(z2, x1),
		Vector3f(x1, y1, z2), Vector3f(z2, y1), Vector3f(x1, y1, z1), Vector3f(z1, y1),
		Vector3f(x1, y2, z1), Vector3f(z1, y2), Vector3f(x1, y2, z2), Vector3f(z2, y2),
		Vector3f(x2, y1, z2), Vector3f(z2, y1), Vector3f(x2, y1, z1), Vector3f(z1, y1),
		Vector3f(x2, y2, z1), Vector3f(z1, y2), Vector3f(x2, y2, z2), Vector3f(z2, y2),
		Vector3f(x1, y1, z1), Vector3f(x1, y1), Vector3f(x2, y1, z1), Vector3f(x2, y1),
		Vector3f(x2, y2, z1), Vector3f(x2, y2), Vector3f(x1, y2, z1), Vector3f(x1, y2),
		Vector3f(x1, y1, z2), Vector3f(x1, y1), Vector3f(x2, y1, z2), Vector3f(x2, y1),
		Vector3f(x2, y2, z2), Vector3f(x2, y2), Vector3f(x1, y2, z2), Vector3f(x1, y2), };

		uint16_t CubeIndices[] = { 0, 1, 3, 3, 1, 2, 5, 4, 6, 6, 4, 7,
			8, 9, 11, 11, 9, 10, 13, 12, 14, 14, 12, 15,
			16, 17, 19, 19, 17, 18, 21, 20, 22, 22, 20, 23 };

		for (int i = 0; i < 36; i++)
			AddIndex(CubeIndices[i] + (uint16_t)numVertices);

		for (int v = 0; v < 24; v++)
		{
			Vertex vvv; vvv.Pos = Vert[v][0];  vvv.U = Vert[v][1].x; vvv.V = Vert[v][1].y;
			float dist1 = (vvv.Pos - Vector3f(-2, 4, -2)).Length();
			float dist2 = (vvv.Pos - Vector3f(3, 4, -3)).Length();
			float dist3 = (vvv.Pos - Vector3f(-4, 3, 25)).Length();
			int   bri = rand() % 160;
			float RRR = c.R * (bri + 192.0f*(0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
			float GGG = c.G * (bri + 192.0f*(0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
			float BBB = c.B * (bri + 192.0f*(0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
			vvv.C.R = RRR > 255 ? 255 : (unsigned char)RRR;
			vvv.C.G = GGG > 255 ? 255 : (unsigned char)GGG;
			vvv.C.B = BBB > 255 ? 255 : (unsigned char)BBB;
			AddVertex(vvv);
		}
	}

	void Model::Render(Matrix4f view, Matrix4f proj)
	{
		Matrix4f modelmat = GetMatrix();
		Matrix4f mat = (view * modelmat).Transposed();
		Matrix4f transposedProj = proj.Transposed();

		Fill->VShader->SetUniform("View", 16, (float *)&mat);
		Fill->VShader->SetUniform("Proj", 16, (float *)&transposedProj);

		Platform.Context->IASetInputLayout(Fill->InputLayout);
		Platform.Context->IASetIndexBuffer(IndexBuffer->D3DBuffer, DXGI_FORMAT_R16_UINT, 0);
		UINT offset = 0;
		Platform.Context->IASetVertexBuffers(0, 1, &VertexBuffer->D3DBuffer, &Fill->VertexSize, &offset);
		Platform.UniformBufferGen->Refresh(Platform.Context,Fill->VShader->UniformData, Fill->VShader->UniformsSize);
		Platform.Context->VSSetConstantBuffers(0, 1, &Platform.UniformBufferGen->D3DBuffer);
		Platform.Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Platform.Context->VSSetShader(Fill->VShader->D3DVert, NULL, 0);
		Platform.Context->PSSetShader(Fill->PShader->D3DPix, NULL, 0);
		Platform.Context->PSSetSamplers(0, 1, &Fill->SamplerState);
		if (Fill->OneTexture) // The latency box has none
			Platform.Context->PSSetShaderResources(0, 1, &Fill->OneTexture->TexSv);
		Platform.Context->DrawIndexed((UINT)VertexBuffer->Size, 0, 0);
	}
};

//------------------------------------------------------------------------- 
struct Scene  
{
    int     num_models;
    Model * Models[10];

    void    Add(Model * n)
    { Models[num_models++] = n; }    

	Scene(int reducedVersion) : num_models(0) // Main world
	{
		D3D11_INPUT_ELEMENT_DESC ModelVertexDesc[] =
        { { "Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Model::Vertex, Pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "Color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(Model::Vertex, C), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Model::Vertex, U), D3D11_INPUT_PER_VERTEX_DATA, 0 }, };

		char* VertexShaderSrc =
			"float4x4 Proj, View;"
			"float4 NewCol;"
			"void main(in  float4 Position  : POSITION,    in  float4 Color : COLOR0, in  float2 TexCoord  : TEXCOORD0,"
			"          out float4 oPosition : SV_Position, out float4 oColor: COLOR0, out float2 oTexCoord : TEXCOORD0)"
			"{   oPosition = mul(Proj, mul(View, Position)); oTexCoord = TexCoord; oColor = Color; }";
		char* PixelShaderSrc =
			"Texture2D Texture   : register(t0); SamplerState Linear : register(s0); "
			"float4 main(in float4 Position : SV_Position, in float4 Color: COLOR0, in float2 TexCoord : TEXCOORD0) : SV_Target"
			"{   return Color * Texture.Sample(Linear, TexCoord); }";

		// Construct textures
		static Model::Color tex_pixels[4][256 * 256];
		ShaderFill * generated_texture[4];

		for (int k = 0; k<4; k++)
		{
			for (int j = 0; j < 256; j++)
				for (int i = 0; i < 256; i++)
				{
				if (k == 0) tex_pixels[0][j * 256 + i] = (((i >> 7) ^ (j >> 7)) & 1) ? Model::Color(180, 180, 180, 255) : Model::Color(80, 80, 80, 255);// floor
				if (k == 1) tex_pixels[1][j * 256 + i] = (((j / 4 & 15) == 0) || (((i / 4 & 15) == 0) && ((((i / 4 & 31) == 0) ^ ((j / 4 >> 4) & 1)) == 0))) ?
														 Model::Color(60, 60, 60, 255) : Model::Color(180, 180, 180, 255); //wall
				if (k == 2) tex_pixels[2][j * 256 + i] = (i / 4 == 0 || j / 4 == 0) ? Model::Color(80, 80, 80, 255) : Model::Color(180, 180, 180, 255);// ceiling
				if (k == 3) tex_pixels[3][j * 256 + i] = Model::Color(128, 128, 128, 255);// blank
				}
			TextureBuffer * t = new TextureBuffer(false, Sizei(256, 256), 8, (unsigned char *)tex_pixels[k],1);
			generated_texture[k] = new ShaderFill(ModelVertexDesc, 3, VertexShaderSrc, PixelShaderSrc, t,sizeof(Model::Vertex));
		}
		// Construct geometry
		Model * m = new Model(Vector3f(0, 0, 0), generated_texture[2]);  // Moving box
		m->AddSolidColorBox(0, 0, 0, +1.0f, +1.0f, 1.0f, Model::Color(64, 64, 64));
		m->AllocateBuffers(); Add(m);

		m = new Model(Vector3f(0, 0, 0), generated_texture[1]);  // Walls
		m->AddSolidColorBox(-10.1f, 0.0f, -20.0f, -10.0f, 4.0f, 20.0f, Model::Color(128, 128, 128)); // Left Wall
		m->AddSolidColorBox(-10.0f, -0.1f, -20.1f, 10.0f, 4.0f, -20.0f, Model::Color(128, 128, 128)); // Back Wall
		m->AddSolidColorBox(10.0f, -0.1f, -20.0f, 10.1f, 4.0f, 20.0f, Model::Color(128, 128, 128));  // Right Wall
		m->AllocateBuffers(); Add(m);

		m = new Model(Vector3f(0, 0, 0), generated_texture[0]);  // Floors
		m->AddSolidColorBox(-10.0f, -0.1f, -20.0f, 10.0f, 0.0f, 20.1f, Model::Color(128, 128, 128)); // Main floor
		m->AddSolidColorBox(-15.0f, -6.1f, 18.0f, 15.0f, -6.0f, 30.0f, Model::Color(128, 128, 128));// Bottom floor
		m->AllocateBuffers(); Add(m);

		if (reducedVersion) return;

		m = new Model(Vector3f(0, 0, 0), generated_texture[2]);  // Ceiling
		m->AddSolidColorBox(-10.0f, 4.0f, -20.0f, 10.0f, 4.1f, 20.1f, Model::Color(128, 128, 128));
		m->AllocateBuffers(); Add(m);

		m = new Model(Vector3f(0, 0, 0), generated_texture[3]);  // Fixtures & furniture
		m->AddSolidColorBox(9.5f, 0.75f, 3.0f, 10.1f, 2.5f, 3.1f, Model::Color(96, 96, 96));   // Right side shelf// Verticals
		m->AddSolidColorBox(9.5f, 0.95f, 3.7f, 10.1f, 2.75f, 3.8f, Model::Color(96, 96, 96));   // Right side shelf
		m->AddSolidColorBox(9.55f, 1.20f, 2.5f, 10.1f, 1.30f, 3.75f, Model::Color(96, 96, 96)); // Right side shelf// Horizontals
		m->AddSolidColorBox(9.55f, 2.00f, 3.05f, 10.1f, 2.10f, 4.2f, Model::Color(96, 96, 96)); // Right side shelf
		m->AddSolidColorBox(5.0f, 1.1f, 20.0f, 10.0f, 1.2f, 20.1f, Model::Color(96, 96, 96));   // Right railing   
		m->AddSolidColorBox(-10.0f, 1.1f, 20.0f, -5.0f, 1.2f, 20.1f, Model::Color(96, 96, 96));   // Left railing  
		for (float f = 5.0f; f <= 9.0f; f += 1.0f)
		{
			m->AddSolidColorBox(f, 0.0f, 20.0f, f + 0.1f, 1.1f, 20.1f, Model::Color(128, 128, 128));// Left Bars
			m->AddSolidColorBox(-f, 1.1f, 20.0f, -f - 0.1f, 0.0f, 20.1f, Model::Color(128, 128, 128));// Right Bars
		}
		m->AddSolidColorBox(-1.8f, 0.8f, 1.0f, 0.0f, 0.7f, 0.0f, Model::Color(128, 128, 0)); // Table
		m->AddSolidColorBox(-1.8f, 0.0f, 0.0f, -1.7f, 0.7f, 0.1f, Model::Color(128, 128, 0)); // Table Leg 
		m->AddSolidColorBox(-1.8f, 0.7f, 1.0f, -1.7f, 0.0f, 0.9f, Model::Color(128, 128, 0)); // Table Leg 
		m->AddSolidColorBox(0.0f, 0.0f, 1.0f, -0.1f, 0.7f, 0.9f, Model::Color(128, 128, 0)); // Table Leg 
		m->AddSolidColorBox(0.0f, 0.7f, 0.0f, -0.1f, 0.0f, 0.1f, Model::Color(128, 128, 0)); // Table Leg 
		m->AddSolidColorBox(-1.4f, 0.5f, -1.1f, -0.8f, 0.55f, -0.5f, Model::Color(44, 44, 128)); // Chair Set
		m->AddSolidColorBox(-1.4f, 0.0f, -1.1f, -1.34f, 1.0f, -1.04f, Model::Color(44, 44, 128)); // Chair Leg 1
		m->AddSolidColorBox(-1.4f, 0.5f, -0.5f, -1.34f, 0.0f, -0.56f, Model::Color(44, 44, 128)); // Chair Leg 2
		m->AddSolidColorBox(-0.8f, 0.0f, -0.5f, -0.86f, 0.5f, -0.56f, Model::Color(44, 44, 128)); // Chair Leg 2
		m->AddSolidColorBox(-0.8f, 1.0f, -1.1f, -0.86f, 0.0f, -1.04f, Model::Color(44, 44, 128)); // Chair Leg 2
		m->AddSolidColorBox(-1.4f, 0.97f, -1.05f, -0.8f, 0.92f, -1.10f, Model::Color(44, 44, 128)); // Chair Back high bar

		for (float f = 3.0f; f <= 6.6f; f += 0.4f)
			m->AddSolidColorBox(-3, 0.0f, f, -2.9f, 1.3f, f + 0.1f, Model::Color(64, 64, 64));//Posts

		m->AllocateBuffers(); Add(m);
	}

	// Simple latency box (keep similar vertex format and shader params same, for ease of code)
	Scene() : num_models(0)
	{
		D3D11_INPUT_ELEMENT_DESC ModelVertexDesc[] =
        { { "Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Model::Vertex, Pos), D3D11_INPUT_PER_VERTEX_DATA, 0 }, };

		char* VertexShaderSrc =
			"float4x4 Proj, View;"
			"float4 NewCol;"
			"void main(in float4 Position : POSITION, out float4 oPosition : SV_Position, out float4 oColor: COLOR0)"
			"{   oPosition = mul(Proj, Position); oColor = NewCol; }";
		char* PixelShaderSrc =
			"float4 main(in float4 Position : SV_Position, in float4 Color: COLOR0) : SV_Target"
			"{   return Color ; }";

		Model* m = new Model(Vector3f(0, 0, 0), new ShaderFill(ModelVertexDesc, 3, VertexShaderSrc, PixelShaderSrc, 0, sizeof(Model::Vertex)));
		float scale = 0.04f;  float extra_y = ((float)Platform.WinSize.w / (float)Platform.WinSize.h);
		m->AddSolidColorBox(1 - scale, 1 - (scale*extra_y), -1, 1 + scale, 1 + (scale*extra_y), -1, Model::Color(0, 128, 0));
		m->AllocateBuffers(); Add(m);
	}

	void Render(Matrix4f view, Matrix4f proj)
	{
		for (int i = 0; i < num_models; i++) Models[i]->Render(view,proj);
	}
};

bool DirectX11::InitWindowAndDevice(HINSTANCE hinst, OVR::Recti vp, bool windowed, char *)
{
    WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
    wc.lpszClassName = L"OVRAppWindow";
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProc;
    wc.cbWndExtra = NULL;
    RegisterClassW(&wc);

    DWORD wsStyle = WS_POPUP;
    DWORD sizeDivisor = 1;

    if (windowed)
    {
        wsStyle |= WS_OVERLAPPEDWINDOW; sizeDivisor = 2;
    }
    RECT winSize = { 0, 0, vp.w / sizeDivisor, vp.h / sizeDivisor };
    AdjustWindowRect(&winSize, wsStyle, false);
    Window = CreateWindowW(L"OVRAppWindow", L"OculusRoomTiny", wsStyle | WS_VISIBLE,
        vp.x, vp.y, winSize.right - winSize.left, winSize.bottom - winSize.top,
        NULL, NULL, hinst, NULL);

    if (!Window)
        return(false);
    if (windowed)
        WinSize = vp.GetSize();
    else
    {
        RECT rc; GetClientRect(Window, &rc);
        WinSize = Sizei(rc.right - rc.left, rc.bottom - rc.top);
    }

    IDXGIFactory * DXGIFactory;
    IDXGIAdapter * Adapter;
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&DXGIFactory))))
        return(false);
    if (FAILED(DXGIFactory->EnumAdapters(0, &Adapter)))
        return(false);
    if (FAILED(D3D11CreateDevice(Adapter, Adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        NULL, 0, NULL, 0, D3D11_SDK_VERSION, &Device, NULL, &Context)))
        return(false);

    DXGI_SWAP_CHAIN_DESC scDesc;
    memset(&scDesc, 0, sizeof(scDesc));
    scDesc.BufferCount = 2;
    scDesc.BufferDesc.Width = WinSize.w;
    scDesc.BufferDesc.Height = WinSize.h;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferDesc.RefreshRate.Numerator = 0;
    scDesc.BufferDesc.RefreshRate.Denominator = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow = Window;
    scDesc.SampleDesc.Count = 1;
    scDesc.SampleDesc.Quality = 0;
    scDesc.Windowed = windowed;
    scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

    if (FAILED(DXGIFactory->CreateSwapChain(Device, &scDesc, &SwapChain)))               return(false);
    if (FAILED(SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer))) return(false);
    if (FAILED(Device->CreateRenderTargetView(BackBuffer, NULL, &BackBufferRT)))         return(false);

    MainDepthBuffer = new DepthBuffer(Sizei(WinSize.w, WinSize.h), 1);
    Context->OMSetRenderTargets(1, &BackBufferRT, MainDepthBuffer->TexDsv);
    if (!windowed) SwapChain->SetFullscreenState(1, NULL);
    UniformBufferGen = new DataBuffer(D3D11_BIND_CONSTANT_BUFFER, NULL, 2000);// make sure big enough

    D3D11_RASTERIZER_DESC rs;
    memset(&rs, 0, sizeof(rs));
    rs.AntialiasedLineEnable = rs.DepthClipEnable = true;
    rs.CullMode = D3D11_CULL_BACK;
    rs.FillMode = D3D11_FILL_SOLID;
    ID3D11RasterizerState *  Rasterizer = NULL;
    Device->CreateRasterizerState(&rs, &Rasterizer);
    Context->RSSetState(Rasterizer);

    D3D11_DEPTH_STENCIL_DESC dss;
    memset(&dss, 0, sizeof(dss));
    dss.DepthEnable = true;
    dss.DepthFunc = D3D11_COMPARISON_LESS;
    dss.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ID3D11DepthStencilState * DepthState;
    Device->CreateDepthStencilState(&dss, &DepthState);
    Context->OMSetDepthStencilState(DepthState, 0);

    SetCapture(Window);
    ShowCursor(FALSE);
    return(true);
}

void DirectX11::ClearAndSetRenderTarget(ID3D11RenderTargetView * rendertarget, struct DepthBuffer * depthbuffer, OVR::Recti vp)
{
    float black[] = { 0, 0, 0, 1 };
    Context->OMSetRenderTargets(1, &rendertarget, depthbuffer->TexDsv);
    Context->ClearRenderTargetView(rendertarget, black);
    Context->ClearDepthStencilView(depthbuffer->TexDsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
    D3D11_VIEWPORT D3Dvp;
    D3Dvp.Width = (float)vp.w;    D3Dvp.Height = (float)vp.h;
    D3Dvp.MinDepth = 0;              D3Dvp.MaxDepth = 1;
    D3Dvp.TopLeftX = (float)vp.x;    D3Dvp.TopLeftY = (float)vp.y;
    Context->RSSetViewports(1, &D3Dvp);
}

#define VALIDATE(x, msg) if (!(x)) { MessageBoxA(NULL, (msg), "OculusRoomTiny", MB_ICONERROR | MB_OK); return 0; }

