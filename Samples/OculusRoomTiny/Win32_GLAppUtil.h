/************************************************************************************
Filename    :   Win32_GLAppUtil.h
Content     :   OpenGL and Application/Window setup functionality for RoomTiny
Created     :   October 20th, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2014 Oculus, LLC. All Rights reserved.
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

#include <GL/CAPI_GLE.h>
#include <Extras/OVR_Math.h>
#include <Kernel/OVR_Log.h>

using namespace OVR;

//---------------------------------------------------------------------------------------
struct DepthBuffer
{
	GLuint        texId;

	DepthBuffer(Sizei size, int sampleCount)
	{
        OVR_ASSERT(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

		glGenTextures(1, &texId);
		glBindTexture(GL_TEXTURE_2D, texId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLenum internalFormat = GL_DEPTH_COMPONENT24;
        GLenum type = GL_UNSIGNED_INT;
        if (GLE_ARB_depth_buffer_float)
        {
            internalFormat = GL_DEPTH_COMPONENT32F;
            type = GL_FLOAT;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, size.w, size.h, 0, GL_DEPTH_COMPONENT, type, NULL);
    }
};

//--------------------------------------------------------------------------
struct TextureBuffer
{
	GLuint        texId;
    GLuint        fboId;
	Sizei		  texSize;

	TextureBuffer(bool rendertarget, OVR::Sizei size, int mipLevels, unsigned char * data, int sampleCount)
	{
        OVR_ASSERT(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

		texSize = size;

		glGenTextures(1, &texId);
		glBindTexture(GL_TEXTURE_2D, texId);

        if (rendertarget) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize.w, texSize.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        if (mipLevels > 1)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glGenFramebuffers(1, &fboId);
	}

	Sizei GetSize(void) const
	{
		return texSize;
	}

    void SetAndClearRenderSurface(DepthBuffer * dbuffer)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fboId);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dbuffer->texId, 0);

        glViewport(0, 0, texSize.w, texSize.h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
};

//-------------------------------------------------------------------------------------------
struct OGL
{
	HWND				Window;
	HDC					hDC;
	HGLRC				WglContext;
    OVR::GLEContext     GLEContext;

	GLuint				fboId;

    bool                Key[256];

	bool InitWindowAndDevice( HINSTANCE hInst, Recti vp, bool windowed, char * deviceName )
	{
		WglContext = 0;
		WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L,
						  GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "ORT", NULL };
		RegisterClassEx(&wc);

		Window = CreateWindow( "ORT", "ORT(OpenGL)", WS_POPUP, vp.x, vp.y, vp.w, vp.h,
								  GetDesktopWindow(), NULL, hInst, NULL );

		hDC = GetDC(Window);

		PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARBFunc = NULL;
		PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARBFunc = NULL;
		{
			// First create a context for the purpose of getting access to wglChoosePixelFormatARB / wglCreateContextAttribsARB.
			PIXELFORMATDESCRIPTOR pfd;
			memset(&pfd, 0, sizeof(pfd));

			pfd.nSize = sizeof(pfd);
			pfd.nVersion = 1;
			pfd.iPixelType = PFD_TYPE_RGBA;
			pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
			pfd.cColorBits = 32;
			pfd.cDepthBits = 16;

			int pf = ChoosePixelFormat(hDC, &pfd);
			if (!pf)
			{
				ReleaseDC(Window, hDC);
                return false;
			}

			if (!SetPixelFormat(hDC, pf, &pfd))
			{
				ReleaseDC(Window, hDC);
                return false;
			}

			HGLRC context = wglCreateContext(hDC);
			if (!wglMakeCurrent(hDC, context))
			{
				wglDeleteContext(context);
				ReleaseDC(Window, hDC);
				return false;
			}

			wglChoosePixelFormatARBFunc = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
			wglCreateContextAttribsARBFunc = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
			OVR_ASSERT(wglChoosePixelFormatARBFunc && wglCreateContextAttribsARBFunc);

			wglDeleteContext(context);
		}

		// Now create the real context that we will be using.
		int iAttributes[] = {
			//WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_COLOR_BITS_ARB, 32,
			WGL_DEPTH_BITS_ARB, 16,
			WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
			WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
			0, 0 };

		float fAttributes[] = { 0, 0 };
		int   pf = 0;
		UINT  numFormats = 0;

		if (!wglChoosePixelFormatARBFunc(hDC, iAttributes, fAttributes, 1, &pf, &numFormats))
		{
			ReleaseDC(Window, hDC);
            return false;
		}

		PIXELFORMATDESCRIPTOR pfd;
		memset(&pfd, 0, sizeof(pfd));

		if (!SetPixelFormat(hDC, pf, &pfd))
		{
			ReleaseDC(Window, hDC);
            return false;
		}

		GLint attribs[16];
		int   attribCount = 0;
		int   flags = 0;
		int   profileFlags = 0;

		attribs[attribCount] = 0;

		WglContext = wglCreateContextAttribsARBFunc(hDC, 0, attribs);
		if (!wglMakeCurrent(hDC, WglContext))
		{
			wglDeleteContext(WglContext);
			ReleaseDC(Window, hDC);
            return false;
		}

		OVR::GLEContext::SetCurrentContext(&GLEContext);
		GLEContext.Init();

		ShowWindow(Window, SW_SHOWDEFAULT);

		glGenFramebuffers(1, &fboId);

		glEnable(GL_DEPTH_TEST);
		glFrontFace(GL_CW);
		glEnable(GL_CULL_FACE);

		SetCapture(Platform.Window);

		ShowCursor(FALSE);

        return true;
	}

	void HandleMessages(void)
	{
		MSG msg;
		if( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) )
		{
			if (msg.message == WM_KEYDOWN) Key[msg.wParam] = true;
			if (msg.message == WM_KEYUP)   Key[msg.wParam] = false;
		}	
	}

	void ReleaseWindow(HINSTANCE hInst)
	{
		ReleaseCapture(); 
		ShowCursor(TRUE);

		glDeleteFramebuffers(1, &fboId);

		if (WglContext) {
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(WglContext);
		}

		UnregisterClass( "ORT", hInst );
	}	

} Platform;


//------------------------------------------------------------------------------
struct ShaderFill
{
	GLuint			program;
	TextureBuffer*	texture;

	ShaderFill(GLuint vertex_shader, GLuint pixel_shader,
		TextureBuffer * arg_texture)
	{
		GLuint vShader = vertex_shader;
		GLuint fShader = pixel_shader;
		texture = arg_texture;

		program = glCreateProgram();
		glAttachShader(program, vShader);
		glAttachShader(program, fShader);

		glLinkProgram(program);
		GLint r;
		glGetProgramiv(program, GL_LINK_STATUS, &r);
		if (!r)
		{
			GLchar msg[1024];
			glGetProgramInfoLog(program, sizeof(msg), 0, msg);
			OVR_DEBUG_LOG(("Linking shaders failed: %s\n", msg));
		}

        glDetachShader(program, vShader);
        glDetachShader(program, fShader);
	}
};

//----------------------------------------------------------------
struct VertexBuffer 
{
	GLuint	buffer;

    VertexBuffer(void* vertices, size_t size) 
    {
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}
};

//----------------------------------------------------------------
struct IndexBuffer 
{
	GLuint	buffer;

	IndexBuffer(void* indices, size_t size)
    {
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
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
        Vector3f  Pos;
        Color     C;
        float     U, V;
    };

    Vector3f     Pos;
    Quatf        Rot;
    Matrix4f     Mat;
    int          numVertices, numIndices;
    Vertex       Vertices[2000]; //Note fixed maximum
    uint16_t     Indices[2000];
    ShaderFill * Fill;
    VertexBuffer * vertexBuffer;
    IndexBuffer * indexBuffer;  

    Model(Vector3f arg_pos, ShaderFill * arg_Fill ) { numVertices=0;numIndices=0;Pos = arg_pos; Fill = arg_Fill; }
    Matrix4f& GetMatrix()                           { Mat = Matrix4f(Rot); Mat = Matrix4f::Translation(Pos) * Mat; return Mat;   }
    void AddVertex(const Vertex& v)                 { Vertices[numVertices++] = v;  }
    void AddIndex(uint16_t a)                       { Indices[numIndices++] = a;   }

    void AllocateBuffers()
    {
        vertexBuffer = new VertexBuffer(&Vertices[0], numVertices * sizeof(Vertex));
        indexBuffer  = new IndexBuffer(&Indices[0], numIndices * 2);
    }

    void AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2, Color c)
    {
        Vector3f Vert[][2] =
        {   Vector3f(x1, y2, z1), Vector3f(z1, x1),  Vector3f(x2, y2, z1), Vector3f(z1, x2),
        Vector3f(x2, y2, z2), Vector3f(z2, x2),  Vector3f(x1, y2, z2), Vector3f(z2, x1),
        Vector3f(x1, y1, z1), Vector3f(z1, x1),  Vector3f(x2, y1, z1), Vector3f(z1, x2),
        Vector3f(x2, y1, z2), Vector3f(z2, x2),  Vector3f(x1, y1, z2), Vector3f(z2, x1),
        Vector3f(x1, y1, z2), Vector3f(z2, y1),  Vector3f(x1, y1, z1), Vector3f(z1, y1),
        Vector3f(x1, y2, z1), Vector3f(z1, y2),  Vector3f(x1, y2, z2), Vector3f(z2, y2),
        Vector3f(x2, y1, z2), Vector3f(z2, y1),  Vector3f(x2, y1, z1), Vector3f(z1, y1),
        Vector3f(x2, y2, z1), Vector3f(z1, y2),  Vector3f(x2, y2, z2), Vector3f(z2, y2),
        Vector3f(x1, y1, z1), Vector3f(x1, y1),  Vector3f(x2, y1, z1), Vector3f(x2, y1),
        Vector3f(x2, y2, z1), Vector3f(x2, y2),  Vector3f(x1, y2, z1), Vector3f(x1, y2),
        Vector3f(x1, y1, z2), Vector3f(x1, y1),  Vector3f(x2, y1, z2), Vector3f(x2, y1),
        Vector3f(x2, y2, z2), Vector3f(x2, y2),  Vector3f(x1, y2, z2), Vector3f(x1, y2), };

        uint16_t CubeIndices[] = {0, 1, 3,     3, 1, 2,     5, 4, 6,     6, 4, 7,
            8, 9, 11,    11, 9, 10,   13, 12, 14,  14, 12, 15,
            16, 17, 19,  19, 17, 18,  21, 20, 22,  22, 20, 23 };

        for(int i = 0; i < 36; i++)
            AddIndex(CubeIndices[i] + (uint16_t) numVertices);

        for(int v = 0; v < 24; v++)
        {
            Vertex vvv; vvv.Pos = Vert[v][0];  vvv.U = Vert[v][1].x; vvv.V = Vert[v][1].y;
            float dist1 = (vvv.Pos - Vector3f(-2,4,-2)).Length();
            float dist2 = (vvv.Pos - Vector3f(3,4,-3)).Length();
            float dist3 = (vvv.Pos - Vector3f(-4,3,25)).Length();
            int   bri   = rand() % 160;
			float RRR   = c.R * (bri + 192.0f*(0.65f + 8/dist1 + 1/dist2 + 4/dist3)) / 255.0f;
            float GGG   = c.G * (bri + 192.0f*(0.65f + 8/dist1 + 1/dist2 + 4/dist3)) / 255.0f;
            float BBB   = c.B * (bri + 192.0f*(0.65f + 8/dist1 + 1/dist2 + 4/dist3)) / 255.0f;
            vvv.C.R = RRR > 255 ? 255: (unsigned char) RRR;
            vvv.C.G = GGG > 255 ? 255: (unsigned char) GGG;
            vvv.C.B = BBB > 255 ? 255: (unsigned char) BBB;
            AddVertex(vvv);
        }
    }

    void Render(Matrix4f view, Matrix4f proj)
    {
		Matrix4f combined = proj * view * GetMatrix();

		glUseProgram(Fill->program);
		glUniform1i(glGetUniformLocation(Fill->program, "Texture0"), 0);
		glUniformMatrix4fv(glGetUniformLocation(Fill->program, "matWVP"), 1, GL_TRUE, (FLOAT*)&combined);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Fill->texture->texId);

		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->buffer);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, Pos));
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, C));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, U));

		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, NULL);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glUseProgram(0);
	}
};

//------------------------------------------------------------------------- 
struct Scene  
{
    int     num_models;
    Model * Models[10];

    void    Add(Model * n)
    {   Models[num_models++] = n; }    

    void Render(Matrix4f view, Matrix4f proj)
    {
		for (int i = 0; i < num_models; i++) {
			Models[i]->Render(view, proj);
		}
	}

	GLuint CreateShader(GLenum type, const GLchar* src)
	{
		GLuint shader = glCreateShader(type);

		glShaderSource(shader, 1, &src, NULL);
		glCompileShader(shader);

		GLint r;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &r);
		if (!r)
		{
			GLchar msg[1024];
			glGetShaderInfoLog(shader, sizeof(msg), 0, msg);
			if (msg[0]) {
				OVR_DEBUG_LOG(("Compiling shader failed: %s\n", msg));
			}
			return 0;
		}

		return shader;
	}

    Scene(int reducedVersion) : num_models(0) // Main world
    {
		static const GLchar* VertexShaderSrc =
			"#version 150\n"
			"uniform mat4 matWVP;\n"
			"in      vec4 Position;\n"
			"in      vec4 Color;\n"
			"in      vec2 TexCoord;\n"
			"out     vec2 oTexCoord;\n"
			"out     vec4 oColor;\n"
			"void main()\n"
			"{\n"
			"   gl_Position = (matWVP * Position);\n"
			"   oTexCoord   = TexCoord;\n"
			"   oColor      = Color;\n"
			"}\n";

		static const char* FragmentShaderSrc =
			"#version 150\n"
			"uniform sampler2D Texture0;\n"
			"in      vec4      oColor;\n"
			"in      vec2      oTexCoord;\n"
			"out     vec4      FragColor;\n"
			"void main()\n"
			"{\n"
			"   FragColor = oColor * texture2D(Texture0, oTexCoord);\n"
			"}\n";

		GLuint	vshader = CreateShader(GL_VERTEX_SHADER, VertexShaderSrc);
		GLuint	fshader = CreateShader(GL_FRAGMENT_SHADER, FragmentShaderSrc);

        //Make textures
        ShaderFill * grid_material[4];
        for (int k=0;k<4;k++)
        {
             static DWORD tex_pixels[256*256];
             for (int j=0;j<256;j++)
             for (int i=0;i<256;i++)
                {
                    if (k==0) tex_pixels[j*256+i] = (((i >> 7) ^ (j >> 7)) & 1) ? 0xffb4b4b4 : 0xff505050;// floor
                    if (k==1) tex_pixels[j*256+i] = (((j/4 & 15) == 0) || (((i/4 & 15) == 0) && ((((i/4 & 31) == 0) ^ ((j/4 >> 4) & 1)) == 0)))
                        ? 0xff3c3c3c : 0xffb4b4b4;//wall
                    if (k==2) tex_pixels[j*256+i] = (i/4 == 0 || j/4 == 0)      ? 0xff505050 : 0xffb4b4b4;// ceiling
                    if (k==3) tex_pixels[j*256+i] = 0xff808080;// blank
			 }
	         TextureBuffer * generated_texture = new TextureBuffer(false, Sizei(256,256),4,(unsigned char *)tex_pixels, 1);
             grid_material[k] = new ShaderFill(vshader,fshader,generated_texture);
        }

		glDeleteShader(vshader);
		glDeleteShader(fshader);

        // Construct geometry
        Model * m = new Model(Vector3f(0,0,0),grid_material[2]);  // Moving box
        m->AddSolidColorBox( 0, 0, 0,  +1.0f,  +1.0f, 1.0f,  Model::Color(64,64,64)); 
        m->AllocateBuffers(); Add(m);

        m = new Model(Vector3f(0,0,0),grid_material[1]);  // Walls
        m->AddSolidColorBox( -10.1f,   0.0f,  -20.0f, -10.0f,  4.0f,  20.0f, Model::Color(128,128,128)); // Left Wall
        m->AddSolidColorBox( -10.0f,  -0.1f,  -20.1f,  10.0f,  4.0f, -20.0f, Model::Color(128,128,128)); // Back Wall
        m->AddSolidColorBox(  10.0f,  -0.1f,  -20.0f,  10.1f,  4.0f,  20.0f, Model::Color(128,128,128));  // Right Wall
        m->AllocateBuffers(); Add(m);

        m = new Model(Vector3f(0,0,0),grid_material[0]);  // Floors
        m->AddSolidColorBox( -10.0f,  -0.1f,  -20.0f,  10.0f,  0.0f, 20.1f,  Model::Color(128,128,128)); // Main floor
        m->AddSolidColorBox( -15.0f,  -6.1f,   18.0f,  15.0f, -6.0f, 30.0f,  Model::Color(128,128,128) );// Bottom floor
        m->AllocateBuffers(); Add(m);

        if (reducedVersion) return;

        m = new Model(Vector3f(0,0,0),grid_material[2]);  // Ceiling
        m->AddSolidColorBox( -10.0f,  4.0f,  -20.0f,  10.0f,  4.1f, 20.1f,  Model::Color(128,128,128)); 
        m->AllocateBuffers(); Add(m);

        m = new Model(Vector3f(0,0,0),grid_material[3]);  // Fixtures & furniture
        m->AddSolidColorBox(   9.5f,   0.75f,  3.0f,  10.1f,  2.5f,   3.1f,  Model::Color(96,96,96) );   // Right side shelf// Verticals
        m->AddSolidColorBox(   9.5f,   0.95f,  3.7f,  10.1f,  2.75f,  3.8f,  Model::Color(96,96,96) );   // Right side shelf
        m->AddSolidColorBox(   9.55f,  1.20f,  2.5f,  10.1f,  1.30f,  3.75f,  Model::Color(96,96,96) ); // Right side shelf// Horizontals
        m->AddSolidColorBox(   9.55f,  2.00f,  3.05f,  10.1f,  2.10f,  4.2f,  Model::Color(96,96,96) ); // Right side shelf
        m->AddSolidColorBox(   5.0f,   1.1f,   20.0f,  10.0f,  1.2f,  20.1f, Model::Color(96,96,96) );   // Right railing   
        m->AddSolidColorBox(  -10.0f,  1.1f, 20.0f,   -5.0f,   1.2f, 20.1f, Model::Color(96,96,96) );   // Left railing  
        for (float f=5.0f;f<=9.0f;f+=1.0f)
        {
            m->AddSolidColorBox(   f,   0.0f,   20.0f,   f+0.1f,  1.1f,  20.1f, Model::Color(128,128,128) );// Left Bars
            m->AddSolidColorBox(  -f,   1.1f,   20.0f,  -f-0.1f,  0.0f,  20.1f, Model::Color(128,128,128) );// Right Bars
        }
        m->AddSolidColorBox( -1.8f, 0.8f, 1.0f,   0.0f,  0.7f,  0.0f,   Model::Color(128,128,0)); // Table
        m->AddSolidColorBox( -1.8f, 0.0f, 0.0f,  -1.7f,  0.7f,  0.1f,   Model::Color(128,128,0)); // Table Leg 
        m->AddSolidColorBox( -1.8f, 0.7f, 1.0f,  -1.7f,  0.0f,  0.9f,   Model::Color(128,128,0)); // Table Leg 
        m->AddSolidColorBox(  0.0f, 0.0f, 1.0f,  -0.1f,  0.7f,  0.9f,   Model::Color(128,128,0)); // Table Leg 
        m->AddSolidColorBox(  0.0f, 0.7f, 0.0f,  -0.1f,  0.0f,  0.1f,   Model::Color(128,128,0)); // Table Leg 
        m->AddSolidColorBox( -1.4f, 0.5f, -1.1f, -0.8f,  0.55f, -0.5f,  Model::Color(44,44,128) ); // Chair Set
        m->AddSolidColorBox( -1.4f, 0.0f, -1.1f, -1.34f, 1.0f,  -1.04f, Model::Color(44,44,128) ); // Chair Leg 1
        m->AddSolidColorBox( -1.4f, 0.5f, -0.5f, -1.34f, 0.0f,  -0.56f, Model::Color(44,44,128) ); // Chair Leg 2
        m->AddSolidColorBox( -0.8f, 0.0f, -0.5f, -0.86f, 0.5f,  -0.56f, Model::Color(44,44,128) ); // Chair Leg 2
        m->AddSolidColorBox( -0.8f, 1.0f, -1.1f, -0.86f, 0.0f,  -1.04f, Model::Color(44,44,128) ); // Chair Leg 2
        m->AddSolidColorBox( -1.4f, 0.97f,-1.05f,-0.8f,  0.92f, -1.10f, Model::Color(44,44,128) ); // Chair Back high bar

        for (float f=3.0f;f<=6.6f;f+=0.4f)
            m->AddSolidColorBox( -3,  0.0f, f,   -2.9f, 1.3f, f+0.1f, Model::Color(64,64,64) );//Posts

        m->AllocateBuffers(); Add(m);
    }
};

