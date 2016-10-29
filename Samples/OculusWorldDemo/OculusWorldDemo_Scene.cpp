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

#include <string>


//-------------------------------------------------------------------------------------
// ***** Scene Creation / Loading

void OculusWorldDemoApp::InitMainFilePath()
{
    // We try alternative relative locations for the file.
    const std::string contentBase = pPlatform->GetContentDirectory() + "/" + WORLDDEMO_ASSET_PATH;
    const char* baseDirectories[] = { "",
                                      contentBase.c_str(),
                                      #ifdef SHRDIR
                                          #define STR1(x) #x
                                          #define STR(x)  STR1(x)
                                          STR(SHRDIR) "/OculusWorldDemo/Assets/Tuscany/"
                                      #endif
                                      };
    std::string newPath;

    for(size_t i = 0; i < OVR_ARRAY_COUNT(baseDirectories); ++i)
    {
        newPath  = baseDirectories[i];
        newPath += WORLDDEMO_ASSET_FILE;

        WriteLog("Trying to load the scene at: %s...", newPath.c_str());

        if (SysFile(newPath.c_str()).IsValid())
        {
            WriteLog("Success loading %s", newPath.c_str());
            MainFilePath = newPath;
            return;
        }
    }

    WriteLog("Unable to find any version of %s. Do you have your working directory set right?", WORLDDEMO_ASSET_FILE);
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

Fill* CreateTextureFill(RenderDevice* prender, const std::string& filename, unsigned int fillTextureLoadFlags)
{
    Ptr<File>    imageFile = *new SysFile(filename.c_str());
    Ptr<Texture> imageTex;
    if (imageFile->IsValid())
        imageTex = *LoadTextureTgaTopDown(prender, imageFile, fillTextureLoadFlags, 255);

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


// Solid textures circle
void AddFloorCircleModelVertices(Model* m, float radius)
{
    float	my		   = 0.0f;
    int		totalSteps = 60;

    // Center vertex
    m->AddVertex(0.0f, my, 0.0f, Color(255, 255, 255, 255),
                 0.5f, 0.5f); // u, v	

    for (int i = 0; i < totalSteps; i++)
    {
        float deltaAngle = (MATH_FLOAT_PI * 2.0f) / totalSteps;
        float x = cosf(deltaAngle * i);
        float z = sinf(deltaAngle * i);
        
        m->AddVertex(x * radius, my, z * radius, Color(255, 255, 255, 255),
                     0.5f + x * 0.5f, 0.5f + z * 0.5f); // u, v

        if (i == totalSteps - 1) // last
        {
            m->AddTriangle(uint16_t(i - 1 + 1), 0, 1);
        }
        else if (i != 0)
        {
            m->AddTriangle(uint16_t(i - 1 + 1), 0, uint16_t(i + 1));
        }
    }
}

// Round textured circle with a hole in a middle; looks a bit like footrest
void AddFloorCircleDonutModelVertices(Model* m, float radius)
{
    float	my		   = 0.0f;
    int		totalSteps = 60;

    // "Donut"	
    for (int i = 0; i < totalSteps; i++)
    {
        float deltaAngle = (MATH_FLOAT_PI * 2.0f) / totalSteps;
        float x = cosf(deltaAngle * i);
        float z = sinf(deltaAngle * i);

        m->AddVertex(x * radius, my, z * radius, Color(255, 255, 255, 255),
                     0.5f + x * 0.5f, 0.5f + z * 0.5f); // u, v
        m->AddVertex(x * (radius - 0.1f) , my, z * (radius - 0.1f), Color(255, 255, 255, 255),
                     0.5f + x * 0.5f * ((0.35f-0.1f)/0.35f),
                     0.5f + z * 0.5f * ((0.35f-0.1f)/0.35f)); // u, v

        int t = i * 2;

        if (i == totalSteps - 1) // last
        {
            m->AddTriangle(uint16_t(t), uint16_t(1), uint16_t(t + 1));
            m->AddTriangle(uint16_t(t), uint16_t(0), uint16_t(1));
        }
        else // if (i != 0)
        {
            m->AddTriangle(uint16_t(t), uint16_t(t + 3), uint16_t(t + 1));
            m->AddTriangle(uint16_t(t), uint16_t(t + 2), uint16_t(t + 3));
        }
    }	
}


// Loads the scene data
void OculusWorldDemoApp::PopulateScene(const char *fileName)
{
    ClearScene();

    XmlHandler xmlHandler;
    if(!xmlHandler.ReadFile(fileName, pRender, &MainScene, &CollisionModels, &GroundCollisionModels, SrgbRequested, AnisotropicSample))
    {
        Menu.SetPopupMessage("FILE LOAD FAILED");
        Menu.SetPopupTimeout(10.0f, true);
    }

    MainScene.SetAmbient(Color4f(1.0f, 1.0f, 1.0f, 1.0f));

    std::string mainFilePathNoExtension = MainFilePath;
    StripExtension(mainFilePathNoExtension);

    unsigned int fillTextureLoadFlags = 0;
    fillTextureLoadFlags |= SrgbRequested ? TextureLoad_SrgbAware : 0;
    fillTextureLoadFlags |= AnisotropicSample ? TextureLoad_Anisotropic : 0;

    // 10x10x10 cubes.
    Ptr<Fill> fillR = *CreateTextureFill(pRender, mainFilePathNoExtension + "_greenCube.tga", fillTextureLoadFlags);
    PopulateCubeFieldScene(&GreenCubesScene, fillR.GetPtr(), 10, 10, 10, Vector3f(0.0f, 0.0f, 0.0f), 0.4f);

    Ptr<Fill> fillB = *CreateTextureFill(pRender, mainFilePathNoExtension + "_redCube.tga", fillTextureLoadFlags);
    PopulateCubeFieldScene(&RedCubesScene, fillB.GetPtr(), 10, 10, 10, Vector3f(0.0f, 0.0f, 0.0f), 0.4f);
    
    Ptr<Fill> fillY = *CreateTextureFill(pRender, mainFilePathNoExtension + "_yellowCube.tga", fillTextureLoadFlags);
    PopulateCubeFieldScene(&YellowCubesScene, fillY.GetPtr(), 10, 10, 10, Vector3f(0.0f, 0.0f, 0.0f), 0.4f);

    Ptr<Fill> imageFill = *CreateTextureFill(pRender, mainFilePathNoExtension + "_OculusCube.tga", fillTextureLoadFlags);
    PopulateCubeFieldScene(&OculusCubesScene, imageFill.GetPtr(), 11, 4, 35, Vector3f(0.0f, 0.0f, -6.0f), 0.5f);

    Vector3f blockModelSizeVec = Vector3f(BlockModelSize, BlockModelSize, BlockModelSize);

    // Handy untextured green cube.
    Ptr<Model> smallGreenCubeModel = *Model::CreateBox(Color(0, 255, 0, 255), Vector3f(0.0f, 0.0f, 0.0f), blockModelSizeVec);
    SmallGreenCube.World.Add(smallGreenCubeModel);

    // Textured cubes.
    Ptr<Model> smallOculusCubeModel = *Model::CreateBox(Color(255, 255, 255, 255), Vector3f(0.0f, 0.0f, 0.0f), blockModelSizeVec);
    smallOculusCubeModel->Fill = imageFill;
    SmallOculusCube.World.Add(smallOculusCubeModel);

    Ptr<Model> smallOculusGreenCubeModel = *Model::CreateBox(Color(255, 255, 255, 255), Vector3f(0.0f, 0.0f, 0.0f), blockModelSizeVec);
    smallOculusGreenCubeModel->Fill = fillR;
    SmallOculusGreenCube.World.Add(smallOculusGreenCubeModel);

    Ptr<Model> smallOculusRedCubeModel = *Model::CreateBox(Color(255, 255, 255, 255), Vector3f(0.0f, 0.0f, 0.0f), blockModelSizeVec);
    smallOculusRedCubeModel->Fill = fillB;
    SmallOculusRedCube.World.Add(smallOculusRedCubeModel);

    int textureLoadFlags = 0;
    textureLoadFlags |= SrgbRequested ? TextureLoad_SrgbAware : 0;
    textureLoadFlags |= AnisotropicSample ? TextureLoad_Anisotropic : 0;
    textureLoadFlags |= TextureLoad_MakePremultAlpha;
    textureLoadFlags |= TextureLoad_SwapTextureSet;

    Ptr<File> imageFile = *new SysFile((mainFilePathNoExtension + "_OculusCube.tga").c_str());
    if (imageFile->IsValid())
        TextureOculusCube = *LoadTextureTgaTopDown(pRender, imageFile, textureLoadFlags, 255);

    imageFile = *new SysFile((mainFilePathNoExtension + "_Cockpit_Panel.tga").c_str());
    if (imageFile->IsValid())
        CockpitPanelTexture = *LoadTextureTgaTopDown(pRender, imageFile, textureLoadFlags, 255);

    if (imageFile->IsValid())
        HdcpTexture = *LoadTextureTgaTopDown(pRender, imageFile, textureLoadFlags | TextureLoad_Hdcp, 255);

    XmlHandler xmlHandler2;
    std::string controllerFilename = GetPath(MainFilePath) + "LeftController.xml";
    if (!xmlHandler2.ReadFile(controllerFilename.c_str(), pRender, &ControllerScene, NULL, NULL))
    {
        Menu.SetPopupMessage("CONTROLLER FILE LOAD FAILED");
        Menu.SetPopupTimeout(10.0f, true);
    }
    ControllerScene.AddLight(Vector3f(0, 30.0f, 0), Color4f(1.0f, 1.0f, 1.0f, 1.0f));
    ControllerScene.AddLight(Vector3f(0, -10.0f, 0), Color4f(.2f, .2f, .2f, 1.0f));


    // Load "Floor Circle" models and textures - used to display floor for seated configuration.	
    Ptr<File>	 floorImageFile    = *new SysFile((mainFilePathNoExtension + "_SitFloorConcrete.tga").c_str());
    Ptr<Texture> roundFloorTexture = *LoadTextureTgaTopDown(pRender, floorImageFile, textureLoadFlags, 220);
    if (roundFloorTexture)
        roundFloorTexture->SetSampleMode(Sample_Anisotropic | Sample_Repeat);
    
    Ptr<ShaderFill> fill = *new ShaderFill(*pRender->CreateShaderSet());
    fill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Vertex, VShader_MVP));
    fill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Fragment, FShader_Texture));
    fill->SetTexture(0, roundFloorTexture);

    pRoundFloorModel[0] = *new Model(Prim_Triangles);
    pRoundFloorModel[0]->Fill = fill;
    AddFloorCircleModelVertices(pRoundFloorModel[0], 0.3f);
    OculusRoundFloor[0].World.Add(pRoundFloorModel[0]);

    pRoundFloorModel[1] = *new Model(Prim_Triangles);
    pRoundFloorModel[1]->Fill = fill;
    AddFloorCircleDonutModelVertices(pRoundFloorModel[1], 0.35f);
    OculusRoundFloor[1].World.Add(pRoundFloorModel[1]);

    if (ovr_GetTrackerCount(Session) > 0)
    	PositionalTracker.Init(Session, mainFilePathNoExtension, pRender, SrgbRequested, AnisotropicSample);


}





void OculusWorldDemoApp::PopulatePreloadScene()
{
    // Load-screen screen shot image
    std::string fileName = MainFilePath;
    StripExtension(fileName);

    Ptr<File>    imageFile = *new SysFile((fileName + "_LoadScreen.tga").c_str());
    if (imageFile->IsValid())
        LoadingTexture = *LoadTextureTgaTopDown(pRender, imageFile, TextureLoad_SrgbAware | TextureLoad_SwapTextureSet, 255);
}

void OculusWorldDemoApp::ClearScene()
{
    MainScene.Clear();
    SmallGreenCube.Clear();
    SmallOculusCube.Clear();
    SmallOculusGreenCube.Clear();
    SmallOculusRedCube.Clear();
    GreenCubesScene.Clear();
    RedCubesScene.Clear();
    OculusCubesScene.Clear();
    ControllerScene.Clear();
    BoundaryScene.Clear();
}


//-------------------------------------------------------------------------------------
// ***** Rendering Content


void OculusWorldDemoApp::RenderAnimatedBlocks(ovrEyeType eye, double appTime)
{
    Render::Scene *pBlockScene = &SmallGreenCube;
    switch (BlocksShowMeshType)
    {
    case 0: pBlockScene = &SmallGreenCube; break;
    case 1: pBlockScene = &SmallOculusCube; break;
    case 2: pBlockScene = &SmallOculusGreenCube; break;
    case 3: pBlockScene = &SmallOculusRedCube; break;
    default:
        BlocksShowMeshType = 0;
        break;
    }

    switch (BlocksShowType)
    {
    case 0:
        // No blocks;
        break;

    case 1: // Horizontal circle around a point.
    case 2: // Vertical circle around a point.
        {
            int         numBlocks = BlocksHowMany;
            float       radius = BlocksMovementRadius;
            Matrix4f    scaleUp = Matrix4f::Scaling(BlockScale);
            double      scaledTime = appTime * 0.1 * (double)BlocksSpeed;
            float       fracTime = (float)(scaledTime - floor(scaledTime));

            for (int j = 0; j < 2; j++)
            {
                for (int i = 0; i < numBlocks; i++)
                {
                    Vector3f offset;
                    float angle;
                    switch ( BlocksMovementType )
                    {
                    case 0:
                        // Rotating circle.
                        angle = (((float)i / numBlocks) + fracTime) * (MATH_FLOAT_PI * 2.0f);
                        offset.x = radius * cosf(angle);
                        offset.y = radius * sinf(angle);
                        offset.z = 0.25f;
                        break;
                    case 1:
                        // Back and forth sine.
                        angle = (((float)i / numBlocks)) * (MATH_FLOAT_PI * 2.0f);
                        angle += BlocksMovementScale * cosf(fracTime*(MATH_FLOAT_PI * 2.0f));
                        offset.x = radius * cosf(angle);
                        offset.y = radius * sinf(angle);
                        offset.z = 0.25f;
                        break;
                    case 2:
                        // Back and forth triangle.
                        angle = (((float)i / numBlocks)) * (MATH_FLOAT_PI * 2.0f);
                        if ( fracTime < 0.5f )
                        {
                            angle += BlocksMovementScale * 2.0f * fracTime;
                        }
                        else
                        {
                            angle += BlocksMovementScale * 2.0f * (1.0f - fracTime);
                        }
                        offset.x = radius * cosf(angle);
                        offset.y = radius * sinf(angle);
                        offset.z = 0.25f;
                        break;
                    default:
                        BlocksMovementType = 0;
                    }
                    if ( j == 0 )
                    {
                        offset.x = -offset.x;
                        offset.z = -offset.z;
                    }

                    Vector3f pos;
                    if ( BlocksShowType == 1 )
                    {
                        // Horizontal
                        pos.x = BlocksCenter.x + offset.x;
                        pos.y = BlocksCenter.y + offset.z;
                        pos.z = BlocksCenter.z + offset.y;
                    }
                    else
                    {
                        // Vertical
                        pos.x = BlocksCenter.x + offset.z;
                        pos.y = BlocksCenter.y + offset.x;
                        pos.z = BlocksCenter.z + offset.y;
                    }

                    Matrix4f translate = Matrix4f::Translation(pos);
                    pBlockScene->Render(pRender, ViewFromWorld[eye] * translate * scaleUp);
                }
            }
        }
        break;

        break;

    case 3:
        {
            // Bouncing.
            const int   numBlocks = 10;
            Matrix4f    scaleUp = Matrix4f::Scaling(BlockScale);

            for (int i = 1; i <= numBlocks; i++)
            {
                double scaledTime = (double)BlocksSpeed * appTime / (double)i;
                float fracTime = (float)(scaledTime - floor(scaledTime));

                Vector3f pos = BlocksCenter;
                pos.z -= (float)i;
                pos.y += -1.5f + 4.0f * (2.0f * fracTime * (1.0f - fracTime));
                Matrix4f translate = Matrix4f::Translation(pos);
                pBlockScene->Render(pRender, ViewFromWorld[eye] * translate * scaleUp);
            }
        }
        break;

    default:
        BlocksShowType = 0;
        break;
    }
}

void OculusWorldDemoApp::RenderGrid(ovrEyeType eye, Recti renderViewport)
{
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
    pRender->SetViewport(renderViewport);

    pRender->SetDepthMode(false, false);
    Color cNormal ( 0, 255, 0 );        // Green is the least-smeared color from CA.
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

    // Draw diagonal lines
    {
        float x[2];
        float y[2];
        x[0] = (float)(midX - renderViewport.w);
        x[1] = (float)(midX + renderViewport.w);
        y[0] = (float)(midY - renderViewport.w);
        y[1] = (float)(midY + renderViewport.w);
        pRender->RenderLines(1, cNormal, x, y);
    }
    {
        float x[2];
        float y[2];
        x[0] = (float)(midX + renderViewport.w);
        x[1] = (float)(midX - renderViewport.w);
        y[0] = (float)(midY - renderViewport.w);
        y[1] = (float)(midY + renderViewport.w);
        pRender->RenderLines(1, cNormal, x, y);
    }
}


void OculusWorldDemoApp::RenderControllers(ovrEyeType eye)
{
    if (!HasInputState)
        return;

    pRender->SetCullMode(RenderDevice::Cull_Off);

    if (ConnectedControllerTypes & ovrControllerType_LTouch)
    {
        Matrix4f scaleUp   = Matrix4f::Scaling(1.0f);
        Posef    worldPose = ThePlayer.VirtualWorldTransformfromRealPose(HandPoses[ovrHand_Left], TrackingOriginType);
        Matrix4f playerPose(worldPose);
        ControllerScene.Render(pRender, ViewFromWorld[eye] * playerPose * scaleUp);
    }

    if (ConnectedControllerTypes & ovrControllerType_RTouch)
    {
        Matrix4f scaleUp   = Matrix4f::Scaling(-1.0f, 1.0f, 1.0f);
        Posef    worldPose = ThePlayer.VirtualWorldTransformfromRealPose(HandPoses[ovrHand_Right], TrackingOriginType);
        Matrix4f playerPose(worldPose);
        ControllerScene.Render(pRender, ViewFromWorld[eye] * playerPose * scaleUp);
    }
    pRender->SetCullMode(RenderDevice::Cull_Back);
}

void drawNormalsAndBoxesForTrackedObjects(Ptr<Model>& model, ovrBoundaryTestResult& result)
{
    float normalBoxWidth = 0.02f;
    float normalWidth = 0.005f;
    OVR::Color red(255, 0, 0);

    model->AddBox(red, result.ClosestPoint, Vector3f(normalBoxWidth, normalBoxWidth, normalBoxWidth));

    Vector3f normalStartpoint = result.ClosestPoint;
    Vector3f normalVector = result.ClosestPointNormal;
    Vector3f widthDirection = normalVector.Cross(Vector3f(0.0f, 1.0f, 0.0f));
    Vector3f normalEndpoint;

    // In or out facing
    if (result.ClosestDistance < 0)
    {
        normalEndpoint = (Vector3f)result.ClosestPoint - normalVector * 0.1f;
    }
    else
    {
        normalEndpoint = (Vector3f)result.ClosestPoint + normalVector * 0.1f;
    }

    Vertex v[4];
    v[0] = Vertex(normalStartpoint, red);
    v[1] = Vertex(normalEndpoint, red);
    v[2] = Vertex(normalEndpoint + widthDirection * normalWidth, red);
    v[3] = Vertex(normalStartpoint + widthDirection * normalWidth, red);
    model->AddQuad(v[0], v[3], v[1], v[2]);
}

void OculusWorldDemoApp::PopulateBoundaryScene(Scene* scene)
{
    scene->Clear();

    Ptr<Model> model = *new Model();
    scene->World.Add(model);

    OVR::Color green(0, 255, 0);
    OVR::Color red(255, 0, 0);
    float lineWidth = 0.01f;
    float boxWidth = 0.02f;

    // Get boundary information
    int numPoints = 0;
    ovrVector3f* boundaryPointsOuter;
    ovr_GetBoundaryGeometry(Session, ovrBoundary_Outer, nullptr, &numPoints);
    boundaryPointsOuter = new ovrVector3f[numPoints];
    ovr_GetBoundaryGeometry(Session, ovrBoundary_Outer, boundaryPointsOuter, nullptr);

    for (int i = 0; i < numPoints; i++)
    {
        // Draw a box centered at the boundary point on top of the boundary
        Vector3f drawPoint = boundaryPointsOuter[i];
        drawPoint.y += 2.5f;
        model->AddBox(green, drawPoint, Vector3f(boxWidth, boxWidth, boxWidth));

        // Connect the dots
        Vector3f nextPoint = boundaryPointsOuter[(i + 1) % numPoints];
        nextPoint.y += 2.5f;
        Vector3f toNextPoint = nextPoint - drawPoint;
        Vector3f normal = toNextPoint.Cross(Vector3f(0.0f, 1.0f, 0.0f));

        Vertex v[4];
        v[0] = Vertex(drawPoint, green);
        v[1] = Vertex(nextPoint, green);
        v[2] = Vertex(nextPoint + normal * lineWidth, green);
        v[3] = Vertex(drawPoint + normal * lineWidth, green);
        model->AddQuad(v[0], v[3], v[1], v[2]);
    }

    // Track closest points and normals
    ovrBoundaryTestResult resultHMD;
    ovrBoundaryTestResult resultLeft;
    ovrBoundaryTestResult resultRight;
    ovr_TestBoundary(Session, ovrTrackedDevice_HMD, ovrBoundary_Outer, &resultHMD);
    ovr_TestBoundary(Session, ovrTrackedDevice_LTouch, ovrBoundary_Outer, &resultLeft);
    ovr_TestBoundary(Session, ovrTrackedDevice_RTouch, ovrBoundary_Outer, &resultRight);

    drawNormalsAndBoxesForTrackedObjects(model, resultHMD);
    drawNormalsAndBoxesForTrackedObjects(model, resultLeft);
    drawNormalsAndBoxesForTrackedObjects(model, resultRight);
}

void OculusWorldDemoApp::RenderBoundaryScene(Matrix4f& view)
{
    BoundaryScene.Clear();
    PopulateBoundaryScene(&BoundaryScene);
    pRender->SetCullMode(RenderDevice::Cull_Off);
    BoundaryScene.Render(pRender, view);
    pRender->SetCullMode(RenderDevice::Cull_Back);
}
