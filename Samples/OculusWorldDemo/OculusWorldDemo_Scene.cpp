/************************************************************************************

Filename    :   OculusWorldDemo_Scene.cpp
Content     :   Logic for loading, and creating rendered scene components,
                cube and grid overlays, etc.
Created     :   October 4, 2012
Authors     :   Michael Antonov, Andrew Reisse, Steve LaValle, Dov Katz
				Peter Hoff, Dan Goodman, Bryan Croteau                

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

*************************************************************************************/

#include "OculusWorldDemo.h"


//-------------------------------------------------------------------------------------
// ***** Scene Creation / Loading

void OculusWorldDemoApp::InitMainFilePath()
{
    // We try alternative relative locations for the file.
    const String contentBase = pPlatform->GetContentDirectory() + "/" + WORLDDEMO_ASSET_PATH1;
    const char* baseDirectories[] = { "", contentBase.ToCStr(), WORLDDEMO_ASSET_PATH2, WORLDDEMO_ASSET_PATH3, WORLDDEMO_ASSET_PATH4 };
    String newPath;

    for(size_t i = 0; i < OVR_ARRAY_COUNT(baseDirectories); ++i)
    {
        newPath  = baseDirectories[i];	
        newPath += WORLDDEMO_ASSET_FILE;	

        OVR_DEBUG_LOG(("Trying to load the scene at: %s...", newPath.ToCStr()));

        if (SysFile(newPath).IsValid())
        {
            OVR_DEBUG_LOG(("Success loading %s", newPath.ToCStr()));
            MainFilePath = newPath;
            return;
        }
    }

    OVR_DEBUG_LOG(("Unable to find any version of %s. Do you have your working directory set right?", WORLDDEMO_ASSET_FILE));
}

// Creates a grid of cubes.
void PopulateCubeFieldScene(Scene* scene, Fill* fill,
                            int cubeCountX, int cubeCountY, int cubeCountZ, Vector3f offset,
                            float cubeSpacing = 0.5f, float cubeSize = 0.1f)
{    

    Vector3f corner(-(((cubeCountX-1) * cubeSpacing) + cubeSize) * 0.5f,
                    -(((cubeCountY-1) * cubeSpacing) + cubeSize) * 0.5f,
                    -(((cubeCountZ-1) * cubeSpacing) + cubeSize) * 0.5f);                    
    corner += offset;

    Vector3f pos = corner;
    
    for (int i = 0; i < cubeCountX; i++)
    {
        // Create a new model for each 'plane' of cubes so we don't exceed
        // the vert size limit.
        Ptr<Model> model = *new Model();
        scene->World.Add(model);

        if (fill)
            model->Fill = fill;

        for (int j = 0; j < cubeCountY; j++)
        {
            for (int k = 0; k < cubeCountZ; k++)
            {
                model->AddBox(0xFFFFFFFF, pos, Vector3f(cubeSize, cubeSize, cubeSize));
                pos.z += cubeSpacing;
            }

            pos.z = corner.z;
            pos.y += cubeSpacing;
        }

        pos.y = corner.y;
        pos.x += cubeSpacing;
    }
}

Fill* CreateTextureFill(RenderDevice* prender, const String& filename)
{
    Ptr<File>    imageFile = *new SysFile(filename);
    Ptr<Texture> imageTex;
    if (imageFile->IsValid())
        imageTex = *LoadTextureTga(prender, imageFile);

    // Image is rendered as a single quad.
    ShaderFill* fill = 0;
    if (imageTex)
    {
        imageTex->SetSampleMode(Sample_Anisotropic|Sample_Repeat);
        fill = new ShaderFill(*prender->CreateShaderSet());
        fill->GetShaders()->SetShader(prender->LoadBuiltinShader(Shader_Vertex, VShader_MVP)); 
        fill->GetShaders()->SetShader(prender->LoadBuiltinShader(Shader_Fragment, FShader_Texture)); 
        fill->SetTexture(0, imageTex);            
    }

    return fill;
}


// Loads the scene data
void OculusWorldDemoApp::PopulateScene(const char *fileName)
{    
    XmlHandler xmlHandler;         
    if(!xmlHandler.ReadFile(fileName, pRender, &MainScene, &CollisionModels, &GroundCollisionModels))
    {
        Menu.SetPopupMessage("FILE LOAD FAILED");
        Menu.SetPopupTimeout(10.0f, true);
    }    

    MainScene.SetAmbient(Color4f(1.0f, 1.0f, 1.0f, 1.0f));
    
    // Handy cube.
    Ptr<Model> smallGreenCubeModel = *Model::CreateBox(Color(0, 255, 0, 255), Vector3f(0.0f, 0.0f, 0.0f), Vector3f(0.004f, 0.004f, 0.004f));
    SmallGreenCube.World.Add(smallGreenCubeModel);

    String mainFilePathNoExtension = MainFilePath;
    mainFilePathNoExtension.StripExtension();


    // 10x10x10 cubes.
    Ptr<Fill> fillR = *CreateTextureFill(pRender, mainFilePathNoExtension + "_redCube.tga");
    PopulateCubeFieldScene(&RedCubesScene, fillR.GetPtr(), 10, 10, 10, Vector3f(0.0f, 0.0f, 0.0f), 0.4f);

    // 10x10x10 cubes.
    Ptr<Fill> fillB = *CreateTextureFill(pRender, mainFilePathNoExtension + "_blueCube.tga");
    PopulateCubeFieldScene(&BlueCubesScene, fillB.GetPtr(), 10, 10, 10, Vector3f(0.0f, 0.0f, 0.0f), 0.4f);

	// Anna: OculusWorldDemo/Assets/Tuscany/Tuscany_OculusCube.tga file needs to be added    
    Ptr<Fill> imageFill = *CreateTextureFill(pRender, mainFilePathNoExtension + "_OculusCube.tga");
    PopulateCubeFieldScene(&OculusCubesScene, imageFill.GetPtr(), 11, 4, 35, Vector3f(0.0f, 0.0f, -6.0f), 0.5f);

	
    float r = 0.01f;
    Ptr<Model> purpleCubesModel = *new Model(Prim_Triangles);
	for (int i = 0; i < 10; i++)
		for (int j = 0; j < 10; j++)
			for (int k = 0; k < 10; k++)
	            purpleCubesModel->AddSolidColorBox(i*0.25f-1.25f-r,j*0.25f-1.25f-r,k*0.25f-1.25f-r,
				                                   i*0.25f-1.25f+r,j*0.25f-1.25f+r,k*0.25f-1.25f+r,0xFF9F009F);
}


void OculusWorldDemoApp::PopulatePreloadScene()
{
    // Load-screen screen shot image
    String fileName = MainFilePath;
    fileName.StripExtension();

    Ptr<File>    imageFile = *new SysFile(fileName + "_LoadScreen.tga");
    Ptr<Texture> imageTex;
    if (imageFile->IsValid())
        imageTex = *LoadTextureTga(pRender, imageFile);

    // Image is rendered as a single quad.
    if (imageTex)
    {
        imageTex->SetSampleMode(Sample_Anisotropic|Sample_Repeat);
        Ptr<Model> m = *new Model(Prim_Triangles);        
        m->AddVertex(-0.5f,  0.5f,  0.0f, Color(255,255,255,255), 0.0f, 0.0f);
        m->AddVertex( 0.5f,  0.5f,  0.0f, Color(255,255,255,255), 1.0f, 0.0f);
        m->AddVertex( 0.5f, -0.5f,  0.0f, Color(255,255,255,255), 1.0f, 1.0f);
        m->AddVertex(-0.5f, -0.5f,  0.0f, Color(255,255,255,255), 0.0f, 1.0f);
        m->AddTriangle(2,1,0);
        m->AddTriangle(0,3,2);

        Ptr<ShaderFill> fill = *new ShaderFill(*pRender->CreateShaderSet());
        fill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Vertex, VShader_MVP)); 
        fill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Fragment, FShader_Texture)); 
        fill->SetTexture(0, imageTex);
        m->Fill = fill;

        LoadingScene.World.Add(m);
    }
}

void OculusWorldDemoApp::ClearScene()
{
    MainScene.Clear();
    SmallGreenCube.Clear();
}


//-------------------------------------------------------------------------------------
// ***** Rendering Content


void OculusWorldDemoApp::RenderAnimatedBlocks(ovrEyeType eye, double appTime)
{
    Matrix4f hmdToEyeViewOffset = Matrix4f::Translation(Vector3f(EyeRenderDesc[eye].HmdToEyeViewOffset));

    switch (BlocksShowType)
    {
    case 0:
        // No blocks;
        break;

    case 1:
        {
            // Horizontal circle around your head.
            const int   numBlocks = 10;
            const float radius = 1.0f;
            Matrix4f    scaleUp = Matrix4f::Scaling(20.0f);
            double      scaledTime = appTime * 0.1;
            float       fracTime = (float)(scaledTime - floor(scaledTime));

            for (int j = 0; j < 2; j++)
            {
                for (int i = 0; i < numBlocks; i++)
                {
                    float angle = (((float)i / numBlocks) + fracTime) * (MATH_FLOAT_PI * 2.0f);
                    Vector3f pos;
                    pos.x = BlocksCenter.x + radius * cosf(angle);
                    pos.y = BlocksCenter.y;
                    pos.z = BlocksCenter.z + radius * sinf(angle);
                    if (j == 0)
                    {
                        pos.x = BlocksCenter.x - radius * cosf(angle);
                        pos.y = BlocksCenter.y - 0.5f;
                    }
                    Matrix4f mat = Matrix4f::Translation(pos);
                    SmallGreenCube.Render(pRender, hmdToEyeViewOffset * View * mat * scaleUp);
                }
            }
        }
        break;

    case 2:
        {
            // Vertical circle around your head.
            const int   numBlocks = 10;
            const float radius = 1.0f;
            Matrix4f    scaleUp = Matrix4f::Scaling(20.0f);
            double      scaledTime = appTime * 0.1;
            float       fracTime = (float)(scaledTime - floor(scaledTime));

            for (int j = 0; j < 2; j++)
            {
                for (int i = 0; i < numBlocks; i++)
                {
                    float angle = (((float)i / numBlocks) + fracTime) * (MATH_FLOAT_PI * 2.0f);
                    Vector3f pos;
                    pos.x = BlocksCenter.x;
                    pos.y = BlocksCenter.y + radius * cosf(angle);
                    pos.z = BlocksCenter.z + radius * sinf(angle);
                    if (j == 0)
                    {
                        pos.x = BlocksCenter.x - 0.5f;
                        pos.y = BlocksCenter.y - radius * cosf(angle);
                    }
                    Matrix4f mat = Matrix4f::Translation(pos);
                    SmallGreenCube.Render(pRender, hmdToEyeViewOffset * View * mat * scaleUp);
                }
            }
        }
        break;

    case 3:
        {
            // Bouncing.
            const int   numBlocks = 10;
            Matrix4f    scaleUp = Matrix4f::Scaling(20.0f);

            for (int i = 1; i <= numBlocks; i++)
            {
                double scaledTime = 4.0f * appTime / (double)i;
                float fracTime = (float)(scaledTime - floor(scaledTime));

                Vector3f pos = BlocksCenter;
                pos.z += (float)i;
                pos.y += -1.5f + 4.0f * (2.0f * fracTime * (1.0f - fracTime));
                Matrix4f mat = Matrix4f::Translation(pos);
                SmallGreenCube.Render(pRender, hmdToEyeViewOffset * View * mat * scaleUp);
            }
        }
        break;

    default:
        BlocksShowType = 0;
        break;
    }
}

void OculusWorldDemoApp::RenderGrid(ovrEyeType eye)
{
    Recti renderViewport = EyeTexture[eye].Header.RenderViewport;    

    // Draw actual pixel grid on the RT.
    // 1:1 mapping to screen pixels, origin in top-left.
    Matrix4f ortho;
    ortho.SetIdentity();
    ortho.M[0][0] = 2.0f / (renderViewport.w);       // X scale
    ortho.M[0][3] = -1.0f;                           // X offset
    ortho.M[1][1] = -2.0f / (renderViewport.h);      // Y scale (for Y=down)
    ortho.M[1][3] = 1.0f;                            // Y offset (Y=down)
    ortho.M[2][2] = 0;
    pRender->SetProjection(ortho);

    pRender->SetDepthMode(false, false);
    Color cNormal ( 255, 0, 0 );
    Color cSpacer ( 255, 255, 0 );
    Color cMid ( 0, 128, 255 );

    int lineStep = 1;
    int midX = 0;
    int midY = 0;
    int limitX = 0;
    int limitY = 0;

    switch ( GridMode )
    {
    case Grid_Rendertarget4:
        lineStep = 4;
        midX    = renderViewport.w / 2;
        midY    = renderViewport.h / 2;
        limitX  = renderViewport.w / 2;
        limitY  = renderViewport.h / 2;
        break;
    case Grid_Rendertarget16:
        lineStep = 16;
        midX    = renderViewport.w / 2;
        midY    = renderViewport.h / 2;
        limitX  = renderViewport.w / 2;
        limitY  = renderViewport.h / 2;
        break;
    case Grid_Lens:
        {                           
            lineStep = 48;
            Vector2f rendertargetNDC = FovPort(EyeRenderDesc[eye].Fov).TanAngleToRendertargetNDC(Vector2f(0.0f));
            midX    = (int)( ( rendertargetNDC.x * 0.5f + 0.5f ) * (float)renderViewport.w + 0.5f );
            midY    = (int)( ( rendertargetNDC.y * 0.5f + 0.5f ) * (float)renderViewport.h + 0.5f );
            limitX  = Alg::Max ( renderViewport.w - midX, midX );
            limitY  = Alg::Max ( renderViewport.h - midY, midY );
        }
        break;
    default: OVR_ASSERT ( false ); break;
    }

    int spacerMask = (lineStep<<2)-1;


    for ( int xp = 0; xp < limitX; xp += lineStep )
    {
        float x[4];
        float y[4];
        x[0] = (float)( midX + xp );
        y[0] = (float)0;
        x[1] = (float)( midX + xp );
        y[1] = (float)renderViewport.h;
        x[2] = (float)( midX - xp );
        y[2] = (float)0;
        x[3] = (float)( midX - xp );
        y[3] = (float)renderViewport.h;
        if ( xp == 0 )
        {
            pRender->RenderLines ( 1, cMid, x, y );
        }
        else if ( ( xp & spacerMask ) == 0 )
        {
            pRender->RenderLines ( 2, cSpacer, x, y );
        }
        else
        {
            pRender->RenderLines ( 2, cNormal, x, y );
        }
    }
    for ( int yp = 0; yp < limitY; yp += lineStep )
    {
        float x[4];
        float y[4];
        x[0] = (float)0;
        y[0] = (float)( midY + yp );
        x[1] = (float)renderViewport.w;
        y[1] = (float)( midY + yp );
        x[2] = (float)0;
        y[2] = (float)( midY - yp );
        x[3] = (float)renderViewport.w;
        y[3] = (float)( midY - yp );
        if ( yp == 0 )
        {
            pRender->RenderLines ( 1, cMid, x, y );
        }
        else if ( ( yp & spacerMask ) == 0 )
        {
            pRender->RenderLines ( 2, cSpacer, x, y );
        }
        else
        {
            pRender->RenderLines ( 2, cNormal, x, y );
        }
    }
}

