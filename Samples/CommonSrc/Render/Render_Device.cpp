/************************************************************************************

Filename    :   Render_Device.cpp
Content     :   Platform renderer for simple scene graph - implementation
Created     :   September 6, 2012
Authors     :   Andrew Reisse

Copyright   :   Copyright 2012 Oculus VR, LLC. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "../Render/Render_Device.h"
#include "../Render/Render_Font.h"

#include "Kernel/OVR_Log.h"


namespace OVR { namespace Render {

	void Model::Render(const Matrix4f& ltw, RenderDevice* ren)
	{
		if(Visible)
		{
			AutoGpuProf prof(ren, "Model_Render");
			Matrix4f m = ltw * GetMatrix();
			ren->Render(m, this);
		}
	}

	void Container::Render(const Matrix4f& ltw, RenderDevice* ren)
	{
		Matrix4f m = ltw * GetMatrix();
		for(unsigned i = 0; i < Nodes.GetSize(); i++)
		{
			Nodes[i]->Render(m, ren);
		}
	}

	Matrix4f SceneView::GetViewMatrix() const
	{
		Matrix4f view = Matrix4f(GetOrientation().Conj()) * Matrix4f::Translation(GetPosition());
		return view;
	}

	void LightingParams::Update(const Matrix4f& view, const Vector3f* SceneLightPos)
	{
		Version++;
		for (int i = 0; i < LightCount; i++)
		{
			LightPos[i] = view.Transform(SceneLightPos[i]);
		}
	}

	void Scene::Render(RenderDevice* ren, const Matrix4f& view)
	{
		AutoGpuProf prof(ren, "Scene_Render");

		Lighting.Update(view, LightPos);

		ren->SetLighting(&Lighting);

		World.Render(view, ren);
	}



	uint16_t CubeIndices[] =
	{
		0, 1, 3,
		3, 1, 2,

		5, 4, 6,
		6, 4, 7,

		8, 9, 11,
		11, 9, 10,

		13, 12, 14,
		14, 12, 15,

		16, 17, 19,
		19, 17, 18,

		21, 20, 22,
		22, 20, 23
	};

	// Colors are specified for planes perpendicular to the axis
	// For example, "xColor" is the color of the y-z plane
	Model* Model::CreateAxisFaceColorBox(float x1, float x2, Color xcolor,
		float y1, float y2, Color ycolor,
		float z1, float z2, Color zcolor)
	{
		float t;

		if(x1 > x2)
		{
			t = x1;
			x1 = x2;
			x2 = t;
		}
		if(y1 > y2)
		{
			t = y1;
			y1 = y2;
			y2 = t;
		}
		if(z1 > z2)
		{
			t = z1;
			z1 = z2;
			z2 = t;
		}

		Model* box = new Model();

		uint16_t startIndex = 0;
		// Cube
		startIndex =
			box->AddVertex(Vector3f(x1, y2, z1), ycolor);
		box->AddVertex(Vector3f(x2, y2, z1), ycolor);
		box->AddVertex(Vector3f(x2, y2, z2), ycolor);
		box->AddVertex(Vector3f(x1, y2, z2), ycolor);

		box->AddVertex(Vector3f(x1, y1, z1), ycolor);
		box->AddVertex(Vector3f(x2, y1, z1), ycolor);
		box->AddVertex(Vector3f(x2, y1, z2), ycolor);
		box->AddVertex(Vector3f(x1, y1, z2), ycolor);

		box->AddVertex(Vector3f(x1, y1, z2), xcolor);
		box->AddVertex(Vector3f(x1, y1, z1), xcolor);
		box->AddVertex(Vector3f(x1, y2, z1), xcolor);
		box->AddVertex(Vector3f(x1, y2, z2), xcolor);

		box->AddVertex(Vector3f(x2, y1, z2), xcolor);
		box->AddVertex(Vector3f(x2, y1, z1), xcolor);
		box->AddVertex(Vector3f(x2, y2, z1), xcolor);
		box->AddVertex(Vector3f(x2, y2, z2), xcolor);

		box->AddVertex(Vector3f(x1, y1, z1), zcolor);
		box->AddVertex(Vector3f(x2, y1, z1), zcolor);
		box->AddVertex(Vector3f(x2, y2, z1), zcolor);
		box->AddVertex(Vector3f(x1, y2, z1), zcolor);

		box->AddVertex(Vector3f(x1, y1, z2), zcolor);
		box->AddVertex(Vector3f(x2, y1, z2), zcolor);
		box->AddVertex(Vector3f(x2, y2, z2), zcolor);
		box->AddVertex(Vector3f(x1, y2, z2), zcolor);


		enum
		{
			//  CubeVertexCount = sizeof(CubeVertices)/sizeof(CubeVertices[0]),
			CubeIndexCount  = sizeof(CubeIndices) / sizeof(CubeIndices[0])
		};

		// Renumber indices
		for(int i = 0; i < CubeIndexCount / 3; i++)
		{
			box->AddTriangle(CubeIndices[i * 3] + startIndex,
				CubeIndices[i * 3 + 1] + startIndex,
				CubeIndices[i * 3 + 2] + startIndex);
		}

		return box;
	}

	void Model::AddSolidColorBox(float x1, float y1, float z1,
		float x2, float y2, float z2,
		Color c)
	{
		float t;

		if(x1 > x2)
		{
			t = x1;
			x1 = x2;
			x2 = t;
		}
		if(y1 > y2)
		{
			t = y1;
			y1 = y2;
			y2 = t;
		}
		if(z1 > z2)
		{
			t = z1;
			z1 = z2;
			z2 = t;
		}

		// Cube vertices and their normals.
		Vector3f CubeVertices[][3] =
		{
            { Vector3f(x1, y2, z1), Vector3f(z1, x1), Vector3f(0.0f, 1.0f, 0.0f) },
            { Vector3f(x2, y2, z1), Vector3f(z1, x2), Vector3f(0.0f, 1.0f, 0.0f) },
            { Vector3f(x2, y2, z2), Vector3f(z2, x2), Vector3f(0.0f, 1.0f, 0.0f) },
            { Vector3f(x1, y2, z2), Vector3f(z2, x1), Vector3f(0.0f, 1.0f, 0.0f) },

            { Vector3f(x1, y1, z1), Vector3f(z1, x1), Vector3f(0.0f, -1.0f, 0.0f) },
            { Vector3f(x2, y1, z1), Vector3f(z1, x2), Vector3f(0.0f, -1.0f, 0.0f) },
            { Vector3f(x2, y1, z2), Vector3f(z2, x2), Vector3f(0.0f, -1.0f, 0.0f) },
            { Vector3f(x1, y1, z2), Vector3f(z2, x1), Vector3f(0.0f, -1.0f, 0.0f) },

            { Vector3f(x1, y1, z2), Vector3f(z2, y1), Vector3f(-1.0f, 0.0f, 0.0f) },
            { Vector3f(x1, y1, z1), Vector3f(z1, y1), Vector3f(-1.0f, 0.0f, 0.0f) },
            { Vector3f(x1, y2, z1), Vector3f(z1, y2), Vector3f(-1.0f, 0.0f, 0.0f) },
            { Vector3f(x1, y2, z2), Vector3f(z2, y2), Vector3f(-1.0f, 0.0f, 0.0f) },

            { Vector3f(x2, y1, z2), Vector3f(z2, y1), Vector3f(1.0f, 0.0f, 0.0f) },
            { Vector3f(x2, y1, z1), Vector3f(z1, y1), Vector3f(1.0f, 0.0f, 0.0f) },
            { Vector3f(x2, y2, z1), Vector3f(z1, y2), Vector3f(1.0f, 0.0f, 0.0f) },
            { Vector3f(x2, y2, z2), Vector3f(z2, y2), Vector3f(1.0f, 0.0f, 0.0f) },

            { Vector3f(x1, y1, z1), Vector3f(x1, y1), Vector3f(0.0f, 0.0f, -1.0f) },
            { Vector3f(x2, y1, z1), Vector3f(x2, y1), Vector3f(0.0f, 0.0f, -1.0f) },
            { Vector3f(x2, y2, z1), Vector3f(x2, y2), Vector3f(0.0f, 0.0f, -1.0f) },
            { Vector3f(x1, y2, z1), Vector3f(x1, y2), Vector3f(0.0f, 0.0f, -1.0f) },

            { Vector3f(x1, y1, z2), Vector3f(x1, y1), Vector3f(0.0f, 0.0f, 1.0f) },
            { Vector3f(x2, y1, z2), Vector3f(x2, y1), Vector3f(0.0f, 0.0f, 1.0f) },
            { Vector3f(x2, y2, z2), Vector3f(x2, y2), Vector3f(0.0f, 0.0f, 1.0f) },
            { Vector3f(x1, y2, z2), Vector3f(x1, y2), Vector3f(0.0f, 0.0f, 1.0f) }
		};


		uint16_t startIndex = GetNextVertexIndex();

		enum
		{
			CubeVertexCount = sizeof(CubeVertices) / sizeof(CubeVertices[0]),
			CubeIndexCount  = sizeof(CubeIndices) / sizeof(CubeIndices[0])
		};

		for(int v = 0; v < CubeVertexCount; v++)
		{
			AddVertex(Vertex(CubeVertices[v][0], c, CubeVertices[v][1].x, CubeVertices[v][1].y, CubeVertices[v][2]));
		}

		// Renumber indices
		for(int i = 0; i < CubeIndexCount / 3; i++)
		{
			AddTriangle(CubeIndices[i * 3] + startIndex,
				CubeIndices[i * 3 + 1] + startIndex,
				CubeIndices[i * 3 + 2] + startIndex);
		}
	}

	// Adds box at specified location to current vertices.
	void Model::AddBox(Color c, Vector3f origin, Vector3f size)
	{    
		Vector3f s = size * 0.5f;
		Vector3f o = origin;
		uint16_t i = GetNextVertexIndex();

		AddVertex(-s.x + o.x,  s.y + o.y, -s.z + o.z,  c, 1, 0, 0, 0, -1);
		AddVertex(s.x  + o.x,  s.y + o.y, -s.z + o.z,  c, 0, 0, 0, 0, -1);
		AddVertex(s.x  + o.x, -s.y + o.y, -s.z + o.z,  c, 0, 1, 0, 0, -1);
		AddVertex(-s.x + o.x, -s.y + o.y, -s.z + o.z,  c, 1, 1, 0, 0, -1);
		AddTriangle(2 + i, 1 + i, 0 + i);
		AddTriangle(0 + i, 3 + i, 2 + i);

		AddVertex(s.x + o.x,  s.y + o.y,  s.z + o.z,  c, 1, 0, 0, 0, 1);
		AddVertex(-s.x+ o.x,  s.y + o.y,  s.z + o.z,  c, 0, 0, 0, 0, 1);
		AddVertex(-s.x+ o.x, -s.y + o.y,  s.z + o.z,  c, 0, 1, 0, 0, 1);
		AddVertex(s.x + o.x, -s.y + o.y,  s.z + o.z,  c, 1, 1, 0, 0, 1);
		AddTriangle(6 + i, 5 + i, 4 + i);
		AddTriangle(4 + i, 7 + i, 6 + i);

		AddVertex(-s.x + o.x,  s.y + o.y, -s.z + o.z,  c, 1, 0, -1, 0, 0);
		AddVertex(-s.x + o.x,  s.y + o.y,  s.z + o.z,  c, 1, 1, -1, 0, 0);
		AddVertex(-s.x + o.x, -s.y + o.y,  s.z + o.z,  c, 0, 1, -1, 0, 0);
		AddVertex(-s.x + o.x, -s.y + o.y, -s.z + o.z,  c, 0, 0, -1, 0, 0);
		AddTriangle(10 + i, 11 + i, 8  + i);
		AddTriangle(8  + i, 9  + i, 10 + i);

		AddVertex(s.x + o.x,  s.y + o.y, -s.z + o.z,  c, 1, 1, 1, 0, 0);
		AddVertex(s.x + o.x, -s.y + o.y, -s.z + o.z,  c, 0, 1, 1, 0, 0);
		AddVertex(s.x + o.x, -s.y + o.y,  s.z + o.z,  c, 0, 0, 1, 0, 0);
		AddVertex(s.x + o.x,  s.y + o.y,  s.z + o.z,  c, 1, 0, 1, 0, 0);
		AddTriangle(14 + i, 15 + i, 12 + i);
		AddTriangle(12 + i, 13 + i, 14 + i);

		AddVertex(-s.x+ o.x, -s.y + o.y,  s.z + o.z,  c, 0, 0, 0, -1, 0);
		AddVertex(s.x + o.x, -s.y + o.y,  s.z + o.z,  c, 1, 0, 0, -1, 0);
		AddVertex(s.x + o.x, -s.y + o.y, -s.z + o.z,  c, 1, 1, 0, -1, 0);
		AddVertex(-s.x+ o.x, -s.y + o.y, -s.z + o.z,  c, 0, 1, 0, -1, 0);
		AddTriangle(18 + i, 19 + i, 16 + i);
		AddTriangle(16 + i, 17 + i, 18 + i);

		AddVertex(-s.x + o.x,  s.y + o.y, -s.z + o.z,  c, 0, 0, 0, 1, 0);
		AddVertex(s.x  + o.x,  s.y + o.y, -s.z + o.z,  c, 1, 0, 0, 1, 0);
		AddVertex(s.x  + o.x,  s.y + o.y,  s.z + o.z,  c, 1, 1, 0, 1, 0);
		AddVertex(-s.x + o.x,  s.y + o.y,  s.z + o.z,  c, 0, 1, 0, 1, 0);
		AddTriangle(20 + i, 21 + i, 22 + i);
		AddTriangle(22 + i, 23 + i, 20 + i);
	}


	Model* Model::CreateBox(Color c, Vector3f origin, Vector3f size)
	{
		Model *box = new Model();
		box->AddBox(c, Vector3f(0), size);
		box->SetPosition(origin);
		return box;
	}

	// Triangulation of a cylinder centered at the origin
	Model* Model::CreateCylinder(Color color, Vector3f origin, float height, float radius, int sides)
	{
		Model *cyl = new Model();
		float halfht = height * 0.5f;
		for(uint16_t i = 0; i < sides; i++)
		{
			float x = cosf(MATH_FLOAT_TWOPI * i / float(sides));
			float y = sinf(MATH_FLOAT_TWOPI * i / float(sides));

			cyl->AddVertex(radius * x, radius * y, halfht, color, x + 1, y, 0, 0, 1);
			cyl->AddVertex(radius * x, radius * y, -1.0f*halfht, color, x, y, 0, 0, -1);

			uint16_t j = 0;
			if(i < sides - 1)
			{
				j = i + 1;
				cyl->AddTriangle(0, i * 4 + 4, i * 4);
				cyl->AddTriangle(1, i * 4 + 1, i * 4 + 5);
			}

			float nx = cosf(MATH_FLOAT_PI * (0.5f + 2.0f * i / float(sides)));
			float ny = sinf(MATH_FLOAT_PI * (0.5f + 2.0f * i / float(sides)));
			cyl->AddVertex(radius * x, radius * y, halfht, color, x + 1, y, nx, ny, 0);
			cyl->AddVertex(radius * x, radius * y, -1.0f*halfht, color, x, y, nx, ny, 0);

			cyl->AddTriangle(i * 4 + 2, j * 4 + 2, i * 4 + 3);
			cyl->AddTriangle(i * 4 + 3, j * 4 + 2, j * 4 + 3);
		}
		cyl->SetPosition(origin);
		return cyl;
	};

	//Triangulation of a cone centered at the origin
	Model* Model::CreateCone(Color color, Vector3f origin, float height, float radius, int sides)
	{
		Model *cone = new Model();
		float halfht = height * 0.5f;
		cone->AddVertex(0.0f, 0.0f, -1.0f*halfht, color, 0, 0, 0, 0, -1);

		for(uint16_t i = 0; i < sides; i++)
		{
			float x = cosf(MATH_FLOAT_TWOPI * i / float(sides));
			float y = sinf(MATH_FLOAT_TWOPI * i / float(sides));

			cone->AddVertex(radius * x, radius * y, -1.0f*halfht, color, 0, 0, 0, 0, -1);

			uint16_t j = 1;
			if(i < sides - 1)
			{
				j = i + 1;
			}

			float next_x = cosf(MATH_FLOAT_TWOPI * j / float(sides));
			float next_y = sinf(MATH_FLOAT_TWOPI *  j / float(sides));

			Vector3f normal = Vector3f(x, y, -halfht).Cross(Vector3f(next_x, next_y, -halfht));

			cone->AddVertex(0.0f, 0.0f, halfht, color, 1, 0, normal.x, normal.y, normal.z); 
			cone->AddVertex(radius * x, radius * y, -1.0f*halfht, color, 0, 0, normal.x, normal.y, normal.z);

			cone->AddTriangle(0, 3*i + 1, 3*j + 1);
			cone->AddTriangle(3*i + 2, 3*j + 3, 3*i + 3);
		}
		cone->SetPosition(origin);
		return cone;
	};

	//Triangulation of a sphere centered at the origin
	Model* Model::CreateSphere(Color color, Vector3f origin, float radius, int sides)
	{
		Model *sphere = new Model();
		uint16_t usides = (uint16_t) sides;
		uint16_t halfsides = usides/2;

		for(uint16_t k = 0; k < halfsides; k++) {

			float z = cosf(MATH_FLOAT_PI * k / float(halfsides));
			float z_r = sinf(MATH_FLOAT_PI * k / float(halfsides)); // the radius of the cross circle with coordinate z

			if (k == 0)  
			{       // add north and south poles
				sphere->AddVertex(0.0f, 0.0f, radius, color, 0, 0, 0, 0, 1);
				sphere->AddVertex(0.0f, 0.0f, -radius, color, 1, 1, 0, 0, -1);
			}
			else 
			{
				for(uint16_t i = 0; i < sides; i++)
				{
					float x = cosf(MATH_FLOAT_TWOPI * i / float(sides)) * z_r;
					float y = sinf(MATH_FLOAT_TWOPI * i / float(sides)) * z_r;

					uint16_t j = 0;
					if(i < sides - 1)
					{
						j = i + 1;
					}

					sphere->AddVertex(radius * x, radius * y, radius * z, color, 0, 1, x, y, z);

					uint16_t indi = 2 + (k -1)*usides + i;
					uint16_t indj = 2 + (k -1)*usides + j;
					if (k == 1) // NorthPole
						sphere->AddTriangle(0, j + 2, i + 2);
					else if (k == halfsides - 1)  //SouthPole
					{
						sphere->AddTriangle(1, indi, indj);
						sphere->AddTriangle(indi, indi - usides, indj);
						sphere->AddTriangle(indi - usides, indj - usides, indj);
					}
					else
					{
						sphere->AddTriangle(indi, indi - usides, indj);
						sphere->AddTriangle(indi - usides, indj - usides, indj);
					}
				}
			} // end else
		}
		sphere->SetPosition(origin);
		return sphere;
	};

	Model* Model::CreateGrid(Vector3f origin, Vector3f stepx, Vector3f stepy,
		int halfx, int halfy, int nmajor, Color minor, Color major)
	{
		Model* grid = new Model(Prim_Lines);
		float  halfxf = (float)halfx;
		float  halfyf = (float)halfy;

		for(int jn = 0; jn <= halfy; jn++)
		{
			float j = (float)jn;

			grid->AddLine(grid->AddVertex((stepx * -halfxf) + (stepy *  j), (jn % nmajor) ? minor : major, 0, 0.5f),
				grid->AddVertex((stepx *  halfxf) + (stepy *  j), (jn % nmajor) ? minor : major, 1, 0.5f));

			if(j)
				grid->AddLine(grid->AddVertex((stepx * -halfxf) + (stepy * -j), (jn % nmajor) ? minor : major, 0, 0.5f),
				grid->AddVertex((stepx *  halfxf) + (stepy * -j), (jn % nmajor) ? minor : major, 1, 0.5f));
		}

		for(int in = 0; in <= halfx; in++)
		{
			float i = (float)in;

			grid->AddLine(grid->AddVertex((stepx *  i) + (stepy * -halfyf), (in % nmajor) ? minor : major, 0, 0.5f),
				grid->AddVertex((stepx *  i) + (stepy *  halfyf), (in % nmajor) ? minor : major, 1, 0.5f));

			if(i)
				grid->AddLine(grid->AddVertex((stepx * -i) + (stepy * -halfyf), (in % nmajor) ? minor : major, 0, 0.5f),
				grid->AddVertex((stepx * -i) + (stepy *  halfyf), (in % nmajor) ? minor : major, 1, 0.5f));
		}

		grid->SetPosition(origin);
		return grid;
	}


	//-------------------------------------------------------------------------------------


	void ShaderFill::Set(PrimitiveType prim) const
	{
		Shaders->Set(prim);

		for(int i = 0; i < 8 && VtxTextures[i] != NULL; ++i)
			VtxTextures[i]->Set(i, Shader_Vertex);

		for(int i = 0; i < 8 && CsTextures[i] != NULL; ++i)
			CsTextures[i]->Set(i, Shader_Compute);

		for (int i = 0; i < 8 && Textures[i] != NULL; ++i)
			Textures[i]->Set(i);
	}



	//-------------------------------------------------------------------------------------
	// ***** Rendering


	RenderDevice::RenderDevice()
		: DistortionClearColor(0, 0, 0),
		TotalTextureMemoryUsage(0),
		FadeOutBorderFraction(0)
	{
		// Ensure these are different, so that the first time it's run, things actually get initialized.
		PostProcessShaderActive = PostProcessShader_Count;
		PostProcessShaderRequested = PostProcessShader_DistortionAndChromAb;
	}

    void RenderDevice::Shutdown()
    {
        // This runs before the subclass's Shutdown(), where the context, etc, may be deleted.
        pTextVertexBuffer.Clear();
        pPostProcessShader.Clear();
        pFullScreenVertexBuffer.Clear();
        pDistortionMeshVertexBuffer[0].Clear();
        pDistortionMeshVertexBuffer[1].Clear();
        pDistortionMeshIndexBuffer[0].Clear();
        pDistortionMeshIndexBuffer[1].Clear();
        pDistortionComputePinBuffer[0].Clear();
        pDistortionComputePinBuffer[1].Clear();
        LightingBuffer.Clear();
    }

	Fill* RenderDevice::CreateTextureFill(Render::Texture* t, bool useAlpha)
	{
		ShaderSet* shaders = CreateShaderSet();
		shaders->SetShader(LoadBuiltinShader(Shader_Vertex, VShader_MVP));
		shaders->SetShader(LoadBuiltinShader(Shader_Fragment, useAlpha ? FShader_AlphaTexture : FShader_Texture));
		Fill* f = new ShaderFill(*shaders);
		f->SetTexture(0, t);
		return f;
	}

	void LightingParams::Set(ShaderSet* s) const
	{
		s->SetUniform4fvArray("Ambient", 1, &Ambient);
		s->SetUniform1f("LightCount", LightCount);
		s->SetUniform4fvArray("LightPos", (int)LightCount, LightPos);
		s->SetUniform4fvArray("LightColor", (int)LightCount, LightColor);
	}

	void RenderDevice::SetLighting(const LightingParams* lt)
	{
		if (!LightingBuffer)
			LightingBuffer = *CreateBuffer();

		LightingBuffer->Data(Buffer_Uniform, lt, sizeof(LightingParams));
		SetCommonUniformBuffer(1, LightingBuffer);
	}

	float RenderDevice::MeasureText(const Font* font, const char* str, float size, float strsize[2],
		const size_t charRange[2], Vector2f charRangeRect[2])
	{
		size_t length = strlen(str);
		float w  = 0;
		float xp = 0;
		float yp = 0;

		for (size_t i = 0; i < length; i++)
		{
			if (str[i] == '\n')
			{
				yp += font->lineheight;
				if(xp > w)
				{
					w = xp;
				}
				xp = 0;
				continue;
			}

			// Record top-left charRange rectangle coordinate.
			if (charRange && charRangeRect && (i == charRange[0]))
				charRangeRect[0] = Vector2f(xp, yp);

			// Tab followed by a numbers sets position to specified offset.
			if (str[i] == '\t')
			{
				char *p = 0;
				float tabPixels = (float)OVR_strtoq(str + i + 1, &p, 10);
				i += p - (str + i + 1);
				xp = tabPixels;
			}
			else
			{
				const Font::Char* ch = &font->chars[(int)str[i]];
				xp += ch->advance;
			}

			// End of character range.
			// Store 'xp' after advance, yp will advance later.
			if (charRange && charRangeRect && (i == charRange[1]))
				charRangeRect[1] = Vector2f(xp, yp);
		}

		if (xp > w)
		{
			w = xp;
		}    

		float scale = (size / font->lineheight);    

		if (strsize)
		{
			strsize[0] = scale * w;
			strsize[1] = scale * (yp + font->lineheight);
		}

		if (charRange && charRangeRect)
		{
			// Selection rectangle ends in teh bottom.
			charRangeRect[1].y += font->lineheight;
			charRangeRect[0] *= scale;
			charRangeRect[1] *= scale;
		}

		return (size / font->lineheight) * w;
	}





	void RenderDevice::RenderText(const Font* font, const char* str,
		float x, float y, float size, Color c, const Matrix4f* view)
	{
		size_t length = strlen(str);

        // Do not attempt to render if we have an empty string.
        if (length == 0) { return; }

		if(!pTextVertexBuffer)
		{
			pTextVertexBuffer = *CreateBuffer();
			if(!pTextVertexBuffer)
			{
				return;
			}
		}

		if(!font->fill)
		{
			font->fill = CreateTextureFill(Ptr<Texture>(
				*CreateTexture(Texture_R, font->twidth, font->theight, font->tex)), true);
		}

		pTextVertexBuffer->Data(Buffer_Vertex, NULL, length * 6 * sizeof(Vertex));
		Vertex* vertices = (Vertex*)pTextVertexBuffer->Map(0, length * 6 * sizeof(Vertex), Map_Discard);
		if(!vertices)
		{
			return;
		}

		Matrix4f m = Matrix4f(size / font->lineheight, 0, 0, 0,
			0, size / font->lineheight, 0, 0,
			0, 0, 0, 0,
			x, y, 0, 1).Transposed();

        if (view)
            m = (*view) * m;

		float xp = 0, yp = (float)font->ascent;
		int   ivertex = 0;

		for (size_t i = 0; i < length; i++)
		{
			if(str[i] == '\n')
			{
				yp += font->lineheight;
				xp = 0;
				continue;
			}
			// Tab followed by a numbers sets position to specified offset.
			if(str[i] == '\t')
			{
				char *p =  0;
				float tabPixels = (float)OVR_strtoq(str + i + 1, &p, 10);
				i += p - (str + i + 1);
				xp = tabPixels;
				continue;
			}

			const Font::Char* ch = &font->chars[(int)str[i]];
			Vertex* chv = &vertices[ivertex];
			for(int j = 0; j < 6; j++)
			{
				chv[j].C = c;
			}
			float x = xp + ch->x;
			float y = yp - ch->y;
			float cx = font->twidth * (ch->u2 - ch->u1);
			float cy = font->theight * (ch->v2 - ch->v1);
			chv[0] = Vertex(Vector3f(x, y, 0), c, ch->u1, ch->v1);
			chv[1] = Vertex(Vector3f(x + cx, y, 0), c, ch->u2, ch->v1);
			chv[2] = Vertex(Vector3f(x + cx, cy + y, 0), c, ch->u2, ch->v2);
			chv[3] = Vertex(Vector3f(x, y, 0), c, ch->u1, ch->v1);
			chv[4] = Vertex(Vector3f(x + cx, cy + y, 0), c, ch->u2, ch->v2);
			chv[5] = Vertex(Vector3f(x, y + cy, 0), c, ch->u1, ch->v2);
			ivertex += 6;

			xp += ch->advance;
		}

		pTextVertexBuffer->Unmap(vertices);

        Render(font->fill, pTextVertexBuffer, NULL, m, 0, ivertex, Prim_Triangles);
	}

	void RenderDevice::FillRect(float left, float top, float right, float bottom, Color c, const Matrix4f* matrix)
	{
		if(!pTextVertexBuffer)
		{
			pTextVertexBuffer = *CreateBuffer();
			if(!pTextVertexBuffer)
			{
				return;
			}
		}

		// Get!!
		Fill* fill = CreateSimpleFill();

		pTextVertexBuffer->Data(Buffer_Vertex, NULL, 6 * sizeof(Vertex));
		Vertex* vertices = (Vertex*)pTextVertexBuffer->Map(0, 6 * sizeof(Vertex), Map_Discard);
		if(!vertices)
		{
			return;
		}

		vertices[0] = Vertex(Vector3f(left,  top,    0.0f), c);
		vertices[1] = Vertex(Vector3f(right, top,    0.0f), c);
		vertices[2] = Vertex(Vector3f(left,  bottom, 0.0f), c);
		vertices[3] = Vertex(Vector3f(left,  bottom, 0.0f), c);
		vertices[4] = Vertex(Vector3f(right, top,    0.0f), c);
		vertices[5] = Vertex(Vector3f(right, bottom, 0.0f), c);

		pTextVertexBuffer->Unmap(vertices);

        if (matrix == NULL)
            Render(fill, pTextVertexBuffer, NULL, Matrix4f(), 0, 6, Prim_Triangles);
        else
            Render(fill, pTextVertexBuffer, NULL, *matrix, 0, 6, Prim_Triangles);
	}



	void RenderDevice::FillGradientRect(float left, float top, float right, float bottom, Color col_top, Color col_btm, const Matrix4f* matrix)
	{
		if(!pTextVertexBuffer)
		{
			pTextVertexBuffer = *CreateBuffer();
			if(!pTextVertexBuffer)
			{
				return;
			}
		}

		// Get!!
		Fill* fill = CreateSimpleFill();

		pTextVertexBuffer->Data(Buffer_Vertex, NULL, 6 * sizeof(Vertex));
		Vertex* vertices = (Vertex*)pTextVertexBuffer->Map(0, 6 * sizeof(Vertex), Map_Discard);
		if(!vertices)
		{
			return;
		}

		vertices[0] = Vertex(Vector3f(left,  top,    0.0f), col_top);
		vertices[1] = Vertex(Vector3f(right, top,    0.0f), col_top);
		vertices[2] = Vertex(Vector3f(left,  bottom, 0.0f), col_btm);
		vertices[3] = Vertex(Vector3f(left,  bottom, 0.0f), col_btm);
		vertices[4] = Vertex(Vector3f(right, top,    0.0f), col_top);
		vertices[5] = Vertex(Vector3f(right, bottom, 0.0f), col_btm);

		pTextVertexBuffer->Unmap(vertices);

        if (matrix)
            Render(fill, pTextVertexBuffer, NULL, *matrix, 0, 6, Prim_Triangles);
        else
            Render(fill, pTextVertexBuffer, NULL, Matrix4f(), 0, 6, Prim_Triangles);
	}


	void RenderDevice::FillTexturedRect(float left, float top, float right, float bottom, float ul, float vt, float ur, float vb, Color c, Ptr<Texture> tex)
	{
		if(!pTextVertexBuffer)
		{
			pTextVertexBuffer = *CreateBuffer();
			if(!pTextVertexBuffer)
			{
				return;
			}
		}

	    Fill *fill = CreateTextureFill(tex, false);
		fill->SetTexture ( 0, tex );

		pTextVertexBuffer->Data(Buffer_Vertex, NULL, 6 * sizeof(Vertex));
		Vertex* vertices = (Vertex*)pTextVertexBuffer->Map(0, 6 * sizeof(Vertex), Map_Discard);
		if(!vertices)
		{
			return;
		}

		vertices[0] = Vertex(Vector3f(left,  top,    0.0f), c, ul, vt);
		vertices[1] = Vertex(Vector3f(right, top,    0.0f), c, ur, vt);
		vertices[2] = Vertex(Vector3f(left,  bottom, 0.0f), c, ul, vb);
		vertices[3] = Vertex(Vector3f(left,  bottom, 0.0f), c, ul, vb);
		vertices[4] = Vertex(Vector3f(right, top,    0.0f), c, ur, vt);
		vertices[5] = Vertex(Vector3f(right, bottom, 0.0f), c, ur, vb);

		pTextVertexBuffer->Unmap(vertices);

		Render(fill, pTextVertexBuffer, NULL, Matrix4f(), 0, 6, Prim_Triangles);
	}


	void RenderDevice::RenderLines ( int NumLines, Color c, float *x, float *y, float *z /*= NULL*/ )
	{
		OVR_ASSERT ( x != NULL );
		OVR_ASSERT ( y != NULL );
		// z can be NULL for 2D stuff.

		if(!pTextVertexBuffer)
		{
			pTextVertexBuffer = *CreateBuffer();
			if(!pTextVertexBuffer)
			{
				return;
			}
		}

		// Get!!
		Fill* fill = CreateSimpleFill();

		int NumVerts = NumLines * 2;

		pTextVertexBuffer->Data(Buffer_Vertex, NULL, NumVerts * sizeof(Vertex));
		Vertex* vertices = (Vertex*)pTextVertexBuffer->Map(0, NumVerts * sizeof(Vertex), Map_Discard);
		if(!vertices)
		{
			return;
		}

		if ( z != NULL )
		{
			for ( int VertNum = 0; VertNum < NumVerts; VertNum++ )
			{
				vertices[VertNum] = Vertex(Vector3f(x[VertNum], y[VertNum], z[VertNum]), c);
			}
		}
		else
		{
			for ( int VertNum = 0; VertNum < NumVerts; VertNum++ )
			{
				vertices[VertNum] = Vertex(Vector3f(x[VertNum], y[VertNum], 1.0f), c);
			}
		}

		pTextVertexBuffer->Unmap(vertices);

		Render(fill, pTextVertexBuffer, NULL, Matrix4f(), 0, NumVerts, Prim_Lines);
	}



	void RenderDevice::RenderImage(float left,
		float top,
		float right,
		float bottom,
		ShaderFill* image,
		unsigned char alpha,
        const Matrix4f* view)
	{
		Color c = Color(255, 255, 255, alpha);
		Ptr<Model> m = *new Model(Prim_Triangles);
		m->AddVertex(left,  bottom,  0.0f, c, 0.0f, 0.0f);
		m->AddVertex(right, bottom,  0.0f, c, 1.0f, 0.0f);
		m->AddVertex(right, top,     0.0f, c, 1.0f, 1.0f);
		m->AddVertex(left,  top,     0.0f, c, 0.0f, 1.0f);
		m->AddTriangle(2,1,0);
		m->AddTriangle(0,3,2);
		m->Fill = image;

        if (view)
            Render(*view, m);
        else
            Render(Matrix4f(), m);
	}

	bool RenderDevice::initPostProcessSupport(PostProcessType pptype)
	{
		if(pptype == PostProcess_None)
		{
			return true;
		}


		if (PostProcessShaderRequested != PostProcessShaderActive)
		{
			pPostProcessShader.Clear();
			PostProcessShaderActive = PostProcessShaderRequested;
		}

		if (!pPostProcessShader)
		{
			Shader *vs = NULL;
			Shader *ppfs = NULL;
			Shader *cs = NULL;
            bool isCompute = false;

			if (PostProcessShaderActive == PostProcessShader_DistortionAndChromAb)
			{
				ppfs = LoadBuiltinShader(Shader_Fragment, FShader_PostProcessWithChromAb);
				vs   = LoadBuiltinShader(Shader_Vertex, VShader_PostProcess);
			}
			else if (PostProcessShaderActive == PostProcessShader_MeshDistortionAndChromAb ||
                     PostProcessShaderActive == PostProcessShader_MeshDistortionAndChromAbHeightmapTimewarp)
			{
				ppfs = LoadBuiltinShader(Shader_Fragment, FShader_PostProcessMeshWithChromAb);
				vs   = LoadBuiltinShader(Shader_Vertex, VShader_PostProcessMesh);
			}
			else if (PostProcessShaderActive == PostProcessShader_MeshDistortionAndChromAbTimewarp)
			{
				ppfs = LoadBuiltinShader(Shader_Fragment, FShader_PostProcessMeshWithChromAbTimewarp);
				vs   = LoadBuiltinShader(Shader_Vertex, VShader_PostProcessMeshTimewarp);
			}
            else if (PostProcessShaderActive == PostProcessShader_MeshDistortionAndChromAbPositionalTimewarp)
            {
                ppfs = LoadBuiltinShader(Shader_Fragment, FShader_PostProcessMeshWithChromAbPositionalTimewarp);
                vs   = LoadBuiltinShader(Shader_Vertex, VShader_PostProcessMeshPositionalTimewarp);
            }
            else if (PostProcessShaderActive == PostProcessShader_Compute2x2Pentile)
            {
                cs = LoadBuiltinShader(Shader_Compute, CShader_PostProcess2x2Pentile);
                isCompute = true;
            }
			else
			{
				OVR_ASSERT(false);
			}

			pPostProcessShader = *CreateShaderSet();
            if ( isCompute )
            {
                OVR_ASSERT ( cs );  // Means the shader failed to compile - look in the debug spew.
    			pPostProcessShader->SetShader(cs);
            }
            else
            {
			    OVR_ASSERT(ppfs); // Means the shader failed to compile - look in the debug spew.
			    OVR_ASSERT(vs);
			    pPostProcessShader->SetShader(vs);
    			pPostProcessShader->SetShader(ppfs);
            }
		}

        // Heightmap method does the timewarp on the first pass
        if (!pPostProcessHeightmapShader &&
            PostProcessShaderActive == PostProcessShader_MeshDistortionAndChromAbHeightmapTimewarp)
        {
            Shader *vs = NULL;
            Shader *ppfs = NULL;

            ppfs = LoadBuiltinShader(Shader_Fragment, FShader_PostProcessHeightmapTimewarp);
            vs   = LoadBuiltinShader(Shader_Vertex, VShader_PostProcessHeightmapTimewarp);

            OVR_ASSERT(ppfs); // Means the shader failed to compile - look in the debug spew.
            OVR_ASSERT(vs);

            pPostProcessHeightmapShader = *CreateShaderSet();
            pPostProcessHeightmapShader->SetShader(vs);
            pPostProcessHeightmapShader->SetShader(ppfs);
        }

		if(!pFullScreenVertexBuffer)
		{
			pFullScreenVertexBuffer = *CreateBuffer();
			const Render::Vertex QuadVertices[] =
			{
				Vertex(Vector3f(0, 1, 0), Color(1, 1, 1, 1), 0, 0),
				Vertex(Vector3f(1, 1, 0), Color(1, 1, 1, 1), 1, 0),
				Vertex(Vector3f(0, 0, 0), Color(1, 1, 1, 1), 0, 1),
				Vertex(Vector3f(1, 0, 0), Color(1, 1, 1, 1), 1, 1)
			};
			pFullScreenVertexBuffer->Data(Buffer_Vertex | Buffer_ReadOnly, QuadVertices, sizeof(QuadVertices));
		}
		return true;
	}

	void RenderDevice::SetProjection(const Matrix4f& proj)
	{
		Proj = proj;
		SetWorldUniforms(proj);
	}

	void RenderDevice::BeginScene(PostProcessType pptype)
	{
		BeginRendering();
		initPostProcessSupport(pptype);
		SetWorldUniforms(Proj);
		SetExtraShaders(NULL);
	}

	void RenderDevice::FinishScene()
	{
		SetExtraShaders(0);
		//SetDefaultRenderTarget();
	}

	bool CollisionModel::TestPoint(const Vector3f& p) const
	{
		for(unsigned i = 0; i < Planes.GetSize(); i++)
			if(Planes[i].TestSide(p) > 0)
			{
				return 0;
			}

			return 1;
	}

	bool CollisionModel::TestRay(const Vector3f& origin, const Vector3f& norm, float& len, Planef* ph) const
	{
		if(TestPoint(origin))
		{
			len = 0;
			*ph = Planes[0];
			return true;
		}
		Vector3f fullMove = origin + norm * len;

		int crossing = -1;
		float cdot1 = 0, cdot2 = 0;

		for(unsigned i = 0; i < Planes.GetSize(); ++i)
		{
			float dot2 = Planes[i].TestSide(fullMove);
			if(dot2 > 0)
			{
				return false;
			}
			float dot1 = Planes[i].TestSide(origin);
			if(dot1 > 0)
			{
				if(dot2 <= 0)
				{
					//OVR_ASSERT(crossing==-1);
					if(crossing == -1)
					{
						crossing = i;
						cdot2 = dot2;
						cdot1 = dot1;
					}
					else
					{
						if(dot2 > cdot2)
						{
							crossing = i;
							cdot2 = dot2;
							cdot1 = dot1;
						}
					}
				}
			}
		}

		if(crossing < 0)
		{
			return false;
		}

		OVR_ASSERT(TestPoint(origin + norm * len));

		len = len * cdot1 / (cdot1 - cdot2) - 0.05f;
		if(len < 0)
		{
			len = 0;
		}
		float tp = Planes[crossing].TestSide(origin + norm * len);
		OVR_ASSERT(fabsf(tp) < 0.05f + MATH_FLOAT_TOLERANCE);
		OVR_UNUSED(tp);

		if(ph)
		{
			*ph = Planes[crossing];
		}
		return true;
	}

	int GetNumMipLevels(int w, int h)
	{
		int n = 1;
		while(w > 1 || h > 1)
		{
			w >>= 1;
			h >>= 1;
			n++;
		}
		return n;
	}

	void FilterRgba2x2(const uint8_t* src, int w, int h, uint8_t* dest)
	{
		for(int j = 0; j < (h & ~1); j += 2)
		{
			const uint8_t* psrc = src + (w * j * 4);
			uint8_t*       pdest = dest + ((w >> 1) * (j >> 1) * 4);

			for(int i = 0; i < w >> 1; i++, psrc += 8, pdest += 4)
			{
				pdest[0] = (((int)psrc[0]) + psrc[4] + psrc[w * 4 + 0] + psrc[w * 4 + 4]) >> 2;
				pdest[1] = (((int)psrc[1]) + psrc[5] + psrc[w * 4 + 1] + psrc[w * 4 + 5]) >> 2;
				pdest[2] = (((int)psrc[2]) + psrc[6] + psrc[w * 4 + 2] + psrc[w * 4 + 6]) >> 2;
				pdest[3] = (((int)psrc[3]) + psrc[7] + psrc[w * 4 + 3] + psrc[w * 4 + 7]) >> 2;
			}
		}
	}

	int GetTextureSize(int format, int w, int h)
	{
		switch (format & Texture_TypeMask)
		{
		case Texture_R:            return w*h;
		case Texture_RGBA:         return w*h*4;
		case Texture_DXT1: {
			int bw = (w+3)/4, bh = (h+3)/4;
			return bw * bh * 8;
						   }
		case Texture_DXT3:
		case Texture_DXT5: {
			int bw = (w+3)/4, bh = (h+3)/4;
			return bw * bh * 16;
						   }

		default:
			OVR_ASSERT(0);
		}
		return 0;
	}

}}
