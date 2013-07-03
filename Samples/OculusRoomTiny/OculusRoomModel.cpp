/************************************************************************************

Filename    :   OculusRoomModel.cpp
Content     :   Creates a simple room scene from hard-coded geometry
Created     :   October 4, 2012

Copyright   :   Copyright 2012-2013 Oculus, Inc. All Rights reserved.

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

#include "RenderTiny_Device.h"

using namespace OVR;
using namespace OVR::RenderTiny;


//-------------------------------------------------------------------------------------
// ***** Room Model

// This model is hard-coded out of axis-aligned solid-colored slabs.
// Room unit dimensions are in meters. Player starts in the middle.
//

enum BuiltinTexture
{
    Tex_None,
    Tex_Checker,
    Tex_Block,
    Tex_Panel,
    Tex_Count
};

struct Slab
{
    float x1, y1, z1;
    float x2, y2, z2;
    Color c;
};

struct SlabModel
{
    int   Count;
    Slab* pSlabs;
    BuiltinTexture tex;
};

Slab FloorSlabs[] =
{
    // Floor
    { -10.0f,  -0.1f,  -20.0f,  10.0f,  0.0f, 20.1f,  Color(128,128,128) }
};

SlabModel Floor = {sizeof(FloorSlabs)/sizeof(Slab), FloorSlabs, Tex_Checker};

Slab CeilingSlabs[] =
{
    { -10.0f,  4.0f,  -20.0f,  10.0f,  4.1f, 20.1f,  Color(128,128,128) }
};

SlabModel Ceiling = {sizeof(FloorSlabs)/sizeof(Slab), CeilingSlabs, Tex_Panel};

Slab RoomSlabs[] =
{
    // Left Wall
    { -10.1f,   0.0f,  -20.0f, -10.0f,  4.0f, 20.0f,  Color(128,128,128) },
    // Back Wall
    { -10.0f,  -0.1f,  -20.1f,  10.0f,  4.0f, -20.0f, Color(128,128,128) },

    // Right Wall
    {  10.0f,  -0.1f,  -20.0f,  10.1f,  4.0f, 20.0f,  Color(128,128,128) },
};

SlabModel Room = {sizeof(RoomSlabs)/sizeof(Slab), RoomSlabs, Tex_Block};

Slab FixtureSlabs[] =
{
    // Right side shelf
    {   9.5f,   0.75f,  3.0f,  10.1f,  2.5f,   3.1f,  Color(128,128,128) }, // Verticals
    {   9.5f,   0.95f,  3.7f,  10.1f,  2.75f,  3.8f,  Color(128,128,128) },
    {   9.5f,   1.20f,  2.5f,  10.1f,  1.30f,  3.8f,  Color(128,128,128) }, // Horizontals
    {   9.5f,   2.00f,  3.0f,  10.1f,  2.10f,  4.2f,  Color(128,128,128) },

    // Right railing    
    {   5.0f,   1.1f,   20.0f,  10.0f,  1.2f,  20.1f, Color(128,128,128) },
    // Bars
    {   9.0f,   1.1f,   20.0f,   9.1f,  0.0f,  20.1f, Color(128,128,128) },
    {   8.0f,   1.1f,   20.0f,   8.1f,  0.0f,  20.1f, Color(128,128,128) },
    {   7.0f,   1.1f,   20.0f,   7.1f,  0.0f,  20.1f, Color(128,128,128) },
    {   6.0f,   1.1f,   20.0f,   6.1f,  0.0f,  20.1f, Color(128,128,128) },
    {   5.0f,   1.1f,   20.0f,   5.1f,  0.0f,  20.1f, Color(128,128,128) },

    // Left railing    
    {  -10.0f,   1.1f, 20.0f,   -5.0f,   1.2f, 20.1f, Color(128,128,128) },
    // Bars
    {  -9.0f,   1.1f,   20.0f,  -9.1f,  0.0f,  20.1f, Color(128,128,128) },
    {  -8.0f,   1.1f,   20.0f,  -8.1f,  0.0f,  20.1f, Color(128,128,128) },
    {  -7.0f,   1.1f,   20.0f,  -7.1f,  0.0f,  20.1f, Color(128,128,128) },
    {  -6.0f,   1.1f,   20.0f,  -6.1f,  0.0f,  20.1f, Color(128,128,128) },
    {  -5.0f,   1.1f,   20.0f,  -5.1f,  0.0f,  20.1f, Color(128,128,128) },

    // Bottom Floor 2
    { -15.0f,  -6.1f,   18.0f,  15.0f, -6.0f, 30.0f,  Color(128,128,128) },
};

SlabModel Fixtures = {sizeof(FixtureSlabs)/sizeof(Slab), FixtureSlabs};

Slab FurnitureSlabs[] =
{
    // Table
    {  -1.8f, 0.7f, 1.0f,  0.0f,      0.8f, 0.0f,      Color(128,128,88) },
    {  -1.8f, 0.7f, 0.0f, -1.8f+0.1f, 0.0f, 0.0f+0.1f, Color(128,128,88) }, // Leg 1
    {  -1.8f, 0.7f, 1.0f, -1.8f+0.1f, 0.0f, 1.0f-0.1f, Color(128,128,88) }, // Leg 2
    {   0.0f, 0.7f, 1.0f,  0.0f-0.1f, 0.0f, 1.0f-0.1f, Color(128,128,88) }, // Leg 2
    {   0.0f, 0.7f, 0.0f,  0.0f-0.1f, 0.0f, 0.0f+0.1f, Color(128,128,88) }, // Leg 2

    // Chair
    {  -1.4f, 0.5f, -1.1f, -0.8f,       0.55f, -0.5f,       Color(88,88,128) }, // Set
    {  -1.4f, 1.0f, -1.1f, -1.4f+0.06f, 0.0f,  -1.1f+0.06f, Color(88,88,128) }, // Leg 1
    {  -1.4f, 0.5f, -0.5f, -1.4f+0.06f, 0.0f,  -0.5f-0.06f, Color(88,88,128) }, // Leg 2
    {  -0.8f, 0.5f, -0.5f, -0.8f-0.06f, 0.0f,  -0.5f-0.06f, Color(88,88,128) }, // Leg 2
    {  -0.8f, 1.0f, -1.1f, -0.8f-0.06f, 0.0f,  -1.1f+0.06f, Color(88,88,128) }, // Leg 2
    {  -1.4f, 0.97f,-1.05f,-0.8f,       0.92f, -1.10f,      Color(88,88,128) }, // Back high bar
};

SlabModel Furniture = {sizeof(FurnitureSlabs)/sizeof(Slab), FurnitureSlabs};

Slab PostsSlabs[] = 
{
    // Posts
    {  0,  0.0f, 0.0f,   0.1f, 1.3f, 0.1f, Color(128,128,128) },
    {  0,  0.0f, 0.4f,   0.1f, 1.3f, 0.5f, Color(128,128,128) },
    {  0,  0.0f, 0.8f,   0.1f, 1.3f, 0.9f, Color(128,128,128) },
    {  0,  0.0f, 1.2f,   0.1f, 1.3f, 1.3f, Color(128,128,128) },
    {  0,  0.0f, 1.6f,   0.1f, 1.3f, 1.7f, Color(128,128,128) },
    {  0,  0.0f, 2.0f,   0.1f, 1.3f, 2.1f, Color(128,128,128) },
    {  0,  0.0f, 2.4f,   0.1f, 1.3f, 2.5f, Color(128,128,128) },
    {  0,  0.0f, 2.8f,   0.1f, 1.3f, 2.9f, Color(128,128,128) },
    {  0,  0.0f, 3.2f,   0.1f, 1.3f, 3.3f, Color(128,128,128) },
    {  0,  0.0f, 3.6f,   0.1f, 1.3f, 3.7f, Color(128,128,128) },
};

SlabModel Posts = {sizeof(PostsSlabs)/sizeof(Slab), PostsSlabs};


// Temporary helper class used to initialize fills used by model. 
class FillCollection
{
public:
    Ptr<ShaderFill> LitSolid;
    Ptr<ShaderFill> LitTextures[4];

    FillCollection(RenderDevice* render);
  
};

FillCollection::FillCollection(RenderDevice* render)
{
    Ptr<Texture> builtinTextures[Tex_Count];

    // Create floor checkerboard texture.
    {
        Color checker[256*256];
        for (int j = 0; j < 256; j++)
            for (int i = 0; i < 256; i++)
                checker[j*256+i] = (((i/4 >> 5) ^ (j/4 >> 5)) & 1) ?
                Color(180,180,180,255) : Color(80,80,80,255);
        builtinTextures[Tex_Checker] = *render->CreateTexture(Texture_RGBA|Texture_GenMipmaps, 256, 256, checker);
        builtinTextures[Tex_Checker]->SetSampleMode(Sample_Anisotropic|Sample_Repeat);
    }

    // Ceiling panel texture.
    {
        Color panel[256*256];
        for (int j = 0; j < 256; j++)
            for (int i = 0; i < 256; i++)
                panel[j*256+i] = (i/4 == 0 || j/4 == 0) ?
                Color(80,80,80,255) : Color(180,180,180,255);
        builtinTextures[Tex_Panel] = *render->CreateTexture(Texture_RGBA|Texture_GenMipmaps, 256, 256, panel);
        builtinTextures[Tex_Panel]->SetSampleMode(Sample_Anisotropic|Sample_Repeat);
    }

    // Wall brick textures.
    {
        Color block[256*256];
        for (int j = 0; j < 256; j++)
            for (int i = 0; i < 256; i++)
                block[j*256+i] = (((j/4 & 15) == 0) || (((i/4 & 15) == 0) && ((((i/4 & 31) == 0) ^ ((j/4 >> 4) & 1)) == 0))) ?
                Color(60,60,60,255) : Color(180,180,180,255);
        builtinTextures[Tex_Block] = *render->CreateTexture(Texture_RGBA|Texture_GenMipmaps, 256, 256, block);
        builtinTextures[Tex_Block]->SetSampleMode(Sample_Anisotropic|Sample_Repeat);
    }

    LitSolid = *new ShaderFill(*render->CreateShaderSet());
    LitSolid->GetShaders()->SetShader(render->LoadBuiltinShader(Shader_Vertex, VShader_MVP)); 
    LitSolid->GetShaders()->SetShader(render->LoadBuiltinShader(Shader_Fragment, FShader_LitGouraud)); 

    for (int i = 1; i < Tex_Count; i++)
    {
        LitTextures[i] = *new ShaderFill(*render->CreateShaderSet());
        LitTextures[i]->GetShaders()->SetShader(render->LoadBuiltinShader(Shader_Vertex, VShader_MVP)); 
        LitTextures[i]->GetShaders()->SetShader(render->LoadBuiltinShader(Shader_Fragment, FShader_LitTexture)); 
        LitTextures[i]->SetTexture(0, builtinTextures[i]);
    }

}



// Helper function to create a model out of Slab arrays.
Model* CreateModel(Vector3f pos, SlabModel* sm, const FillCollection& fills)
{
    Model* m = new Model(Prim_Triangles);
    m->SetPosition(pos);

    for(int i=0; i< sm->Count; i++)
    {
        Slab &s = sm->pSlabs[i];
        m->AddSolidColorBox(s.x1, s.y1, s.z1, s.x2, s.y2, s.z2, s.c);
    }

    if (sm->tex > 0)
        m->Fill = fills.LitTextures[sm->tex];
    else
        m->Fill = fills.LitSolid;
    return m;
}


// Adds sample models and lights to the argument scene.
void PopulateRoomScene(Scene* scene, RenderDevice* render)
{
    FillCollection fills(render);  

    scene->World.Add(Ptr<Model>(*CreateModel(Vector3f(0,0,0),  &Room,       fills)));
    scene->World.Add(Ptr<Model>(*CreateModel(Vector3f(0,0,0),  &Floor,      fills)));
    scene->World.Add(Ptr<Model>(*CreateModel(Vector3f(0,0,0),  &Ceiling,    fills)));
    scene->World.Add(Ptr<Model>(*CreateModel(Vector3f(0,0,0),  &Fixtures,   fills)));
    scene->World.Add(Ptr<Model>(*CreateModel(Vector3f(0,0,0),  &Furniture,  fills)));
    scene->World.Add(Ptr<Model>(*CreateModel(Vector3f(0,0,4),  &Furniture,  fills)));
    scene->World.Add(Ptr<Model>(*CreateModel(Vector3f(-3,0,3), &Posts,      fills)));
  

    scene->SetAmbient(Vector4f(0.65f,0.65f,0.65f,1));
    scene->AddLight(Vector3f(-2,4,-2), Vector4f(8,8,8,1));
    scene->AddLight(Vector3f(3,4,-3),  Vector4f(2,1,1,1));
    scene->AddLight(Vector3f(-4,3,25), Vector4f(3,6,3,1));
}

