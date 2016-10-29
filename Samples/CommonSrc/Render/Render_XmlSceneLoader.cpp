/************************************************************************************

Filename    :   Render_XmlSceneLoader.cpp
Content     :   Imports and exports XML files - implementation
Created     :   January 21, 2013
Authors     :   Robotic Arm Software - Peter Hoff, Dan Goodman, Bryan Croteau

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

#include "Render_XmlSceneLoader.h"
#include "../Util/Logger.h"

namespace OVR { namespace Render {

XmlHandler::XmlHandler() :
    pXmlDocument(NULL),
    textureCount(0),
    modelCount(0),
    collisionModelCount(0),
    groundCollisionModelCount(0)
{
    pXmlDocument = new tinyxml2::XMLDocument();
}

XmlHandler::~XmlHandler()
{
    delete pXmlDocument;
}

bool XmlHandler::ReadFile(const char* fileName, OVR::Render::RenderDevice* pRender,
                          OVR::Render::Scene* pScene,
                          std::vector<Ptr<CollisionModel> >* pCollisions,
                          std::vector<Ptr<CollisionModel> >* pGroundCollisions,
                          bool srgbAware /*= false*/,
                          bool anisotropic /*= false*/)
{
    if(pXmlDocument->LoadFile(fileName) != 0)
    {
        return false;
    }

    // Extract the relative path to our working directory for loading textures
    filePath[0] = 0;
    intptr_t pos = 0;
	intptr_t len = strlen(fileName);
    for(intptr_t i = len; i > 0; i--)
    {
        if (fileName[i-1]=='\\' || fileName[i-1]=='/')
        {
            memcpy(filePath, fileName, i);
            filePath[i] = 0;
            break;
        }        
    }    

    // Load the textures
    WriteLog("Loading textures...");
    XMLElement* pXmlTexture = pXmlDocument->FirstChildElement("scene")->FirstChildElement("textures");
    OVR_ASSERT(pXmlTexture);
    if (pXmlTexture)
    {
        pXmlTexture->QueryIntAttribute("count", &textureCount);
        pXmlTexture = pXmlTexture->FirstChildElement("texture");
    }

    for(int i = 0; i < textureCount; ++i)
    {
        const char* textureName = pXmlTexture->Attribute("fileName");
		intptr_t    dotpos = strcspn(textureName, ".");
        char        fname[300];

		if (pos == len)
		{            
            snprintf(fname, 300, "%s", textureName);
		}
		else
		{
            snprintf(fname, 300, "%s%s", filePath, textureName);
		}

        int textureLoadFlags = 0;
        textureLoadFlags |= srgbAware ? TextureLoad_SrgbAware : 0;
        textureLoadFlags |= anisotropic ? TextureLoad_Anisotropic : 0;

        SysFile* pFile = new SysFile(fname);
		Ptr<Texture> texture;
		if (textureName[dotpos + 1] == 'd' || textureName[dotpos + 1] == 'D')
		{
			// DDS file
            Texture* tmp_ptr = LoadTextureDDSTopDown(pRender, pFile, textureLoadFlags);
			if(tmp_ptr)
			{
				texture.SetPtr(*tmp_ptr);
			}
		}
		else
		{
            Texture* tmp_ptr = LoadTextureTgaTopDown(pRender, pFile, textureLoadFlags, 255);
			if(tmp_ptr)
			{
				texture.SetPtr(*tmp_ptr);
			}
		}

        Textures.push_back(texture);
        pFile->Close();
        pFile->Release();
        pXmlTexture = pXmlTexture->NextSiblingElement("texture");
    }
    WriteLog("Done.\n");

    // Load the models
	pXmlDocument->FirstChildElement("scene")->FirstChildElement("models")->
		          QueryIntAttribute("count", &modelCount);
	
    WriteLog("Loading models... %i models to load...", modelCount);
    XMLElement* pXmlModel = pXmlDocument->FirstChildElement("scene")->
		                                  FirstChildElement("models")->FirstChildElement("model");
    for(int i = 0; i < modelCount; ++i)
    {
		if (i % 15 == 0)
		{
            WriteLog("%i models remaining...", modelCount - i);
		}
        const char* name = pXmlModel->Attribute("name");
        Models.push_back(*new Model(Prim_Triangles, name));
        bool isCollisionModel = false;
        pXmlModel->QueryBoolAttribute("isCollisionModel", &isCollisionModel);
        Models[i]->IsCollisionModel = isCollisionModel;
		if (isCollisionModel)
		{
			Models[i]->Visible = false;
		}

        bool tree_c = (strcmp(name, "tree_C") == 0) || (strcmp(name, "Object03") == 0);

        //read the vertices
        std::vector<Vector3f> *vertices = new std::vector<Vector3f>();
        ParseVectorString(pXmlModel->FirstChildElement("vertices")->FirstChild()->
			              ToText()->Value(), vertices);

		for (size_t vertexIndex = 0; vertexIndex < vertices->size(); ++vertexIndex)
		{
			vertices->at(vertexIndex).x *= -1.0f;

            if (tree_c)
            {   // Move the terrace tree closer to the house
                vertices->at(vertexIndex).z += 0.5;
            }
		}

        //read the normals
        std::vector<Vector3f> *normals = new std::vector<Vector3f>();
        ParseVectorString(pXmlModel->FirstChildElement("normals")->FirstChild()->
			              ToText()->Value(), normals);

		for (size_t normalIndex = 0; normalIndex < normals->size(); ++normalIndex)
		{
			normals->at(normalIndex).z *= -1.0f;
		}

        //read the textures
        std::vector<Vector3f> *diffuseUVs = new std::vector<Vector3f>();
        std::vector<Vector3f> *lightmapUVs = new std::vector<Vector3f>();
        int         diffuseTextureIndex = -1;
        int         lightmapTextureIndex = -1;
        XMLElement* pXmlCurMaterial = pXmlModel->FirstChildElement("material");

        while(pXmlCurMaterial != NULL)
        {
            if(pXmlCurMaterial->Attribute("name", "diffuse"))
            {
                pXmlCurMaterial->FirstChildElement("texture")->
					             QueryIntAttribute("index", &diffuseTextureIndex);
                if(diffuseTextureIndex > -1)
                {
                    ParseVectorString(pXmlCurMaterial->FirstChildElement("texture")->
						              FirstChild()->ToText()->Value(), diffuseUVs, true);
                }
            }
            else if(pXmlCurMaterial->Attribute("name", "lightmap"))
            {
                pXmlCurMaterial->FirstChildElement("texture")->
					                               QueryIntAttribute("index", &lightmapTextureIndex);
                if(lightmapTextureIndex > -1)
                {
                    XMLElement* firstChildElement = pXmlCurMaterial->FirstChildElement("texture");
                    XMLNode* firstChild = firstChildElement->FirstChild();
                    XMLText* text = firstChild->ToText();
                    const char* value = text->Value();
                    ParseVectorString(value, lightmapUVs, true);
                }
            }

            pXmlCurMaterial = pXmlCurMaterial->NextSiblingElement("material");
        }

        //set up the shader
        Ptr<ShaderFill> shader = *new ShaderFill(*pRender->CreateShaderSet());
        shader->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Vertex, VShader_MVP));
        if(diffuseTextureIndex > -1)
        {
            shader->SetTexture(0, Textures[diffuseTextureIndex]);
            if(lightmapTextureIndex > -1)
            {
                shader->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Fragment, FShader_MultiTexture));
                shader->SetTexture(1, Textures[lightmapTextureIndex]);
            }
            else
            {
                shader->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Fragment, FShader_Texture));
            }
        }
        else
        {
            shader->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Fragment, FShader_LitGouraud));
        }
        Models[i]->Fill = shader;

        //add all the vertices to the model
        const size_t numVerts = vertices->size();
        for(size_t v = 0; v < numVerts; ++v)
        {
            if(diffuseTextureIndex > -1)
            {
                if(lightmapTextureIndex > -1)
                {
                    Models[i]->AddVertex(vertices->at(v).z, vertices->at(v).y, vertices->at(v).x, Color(255, 255, 255),
                                          diffuseUVs->at(v).x, diffuseUVs->at(v).y, lightmapUVs->at(v).x, lightmapUVs->at(v).y,
                                          normals->at(v).x, normals->at(v).y, normals->at(v).z);
                }
                else
                {
                    Models[i]->AddVertex(vertices->at(v).z, vertices->at(v).y, vertices->at(v).x, Color(255, 255, 255),
                                          diffuseUVs->at(v).x, diffuseUVs->at(v).y, 0, 0,
                                          normals->at(v).x, normals->at(v).y, normals->at(v).z);
                }
            }
            else
            {
                Models[i]->AddVertex(vertices->at(v).z, vertices->at(v).y, vertices->at(v).x, Color(255, 255, 255, 255),
                                      0, 0, 0, 0,
                                      normals->at(v).x, normals->at(v).y, normals->at(v).z);
            }
        }

        // Read the vertex indices for the triangles
        const char* indexStr = pXmlModel->FirstChildElement("indices")->
                                          FirstChild()->ToText()->Value();
        
        size_t stringLength = strlen(indexStr);

        for(size_t j = 0; j < stringLength; )
        {
            size_t k = j + 1;
            for(; k < stringLength; ++k)
            {
                if (indexStr[k] == ' ')
                    break;                
            }
            char text[20];
            for(size_t l = 0; l < k - j; ++l)
            {
                text[l] = indexStr[j + l];
            }
            text[k - j] = '\0';

            Models[i]->Indices.push_back((unsigned short)atoi(text));
            j = k + 1;
        }

        // Reverse index order to match original expected orientation
        std::vector<uint16_t>& indices    = Models[i]->Indices;
        size_t         indexCount = indices.size();         

        for (size_t revIndex = 0; revIndex < indexCount/2; revIndex++)
        {
            unsigned short itemp               = indices[revIndex];
            indices[revIndex]                  = indices[indexCount - revIndex - 1];
            indices[indexCount - revIndex - 1] = itemp;            
        }

        delete vertices;
        delete normals;
        delete diffuseUVs;
        delete lightmapUVs;

        pScene->World.Add(Models[i]);
        pScene->Models.push_back(Models[i]);
        pXmlModel = pXmlModel->NextSiblingElement("model");
    }
    WriteLog("Done.");

    //load the collision models
    WriteLog("Loading collision models... ");
    XMLElement* pXmlCollisionModel = pXmlDocument->FirstChildElement("scene")->FirstChildElement("collisionModels");
    if (pXmlCollisionModel)
    {
		pXmlCollisionModel->QueryIntAttribute("count", &collisionModelCount);
        pXmlCollisionModel = pXmlCollisionModel->FirstChildElement("collisionModel");
    }

    XMLElement* pXmlPlane = NULL;
    for(int i = 0; i < collisionModelCount; ++i)
    {
        Ptr<CollisionModel> cm = *new CollisionModel();
        int planeCount = 0;
        
        OVR_ASSERT(pXmlCollisionModel != NULL); // collisionModelCount should guarantee this.
        if (pXmlCollisionModel)
        {
        pXmlCollisionModel->QueryIntAttribute("planeCount", &planeCount);

        pXmlPlane = pXmlCollisionModel->FirstChildElement("plane");
        for(int j = 0; j < planeCount; ++j)
        {
            Vector3f norm;
            pXmlPlane->QueryFloatAttribute("nx", &norm.x);
            pXmlPlane->QueryFloatAttribute("ny", &norm.y);
            pXmlPlane->QueryFloatAttribute("nz", &norm.z);
            float D;
            pXmlPlane->QueryFloatAttribute("d", &D);
            D -= 0.5f;
            if (i == 26)
                D += 0.5f;  // tighten the terrace collision so player can move right up to rail
            Planef p(norm.z, norm.y, norm.x * -1.0f, D);
            cm->Add(p);
            pXmlPlane = pXmlPlane->NextSiblingElement("plane");
        }

        if (pCollisions)
        pCollisions->push_back(cm);
        pXmlCollisionModel = pXmlCollisionModel->NextSiblingElement("collisionModel");
    }
    }
    WriteLog("Done.");

    //load the ground collision models
    WriteLog("Loading ground collision models...");
    pXmlCollisionModel = pXmlDocument->FirstChildElement("scene")->FirstChildElement("groundCollisionModels");
    OVR_ASSERT(pXmlCollisionModel);
    if (pXmlCollisionModel)
    {
		pXmlCollisionModel->QueryIntAttribute("count", &groundCollisionModelCount);
        pXmlCollisionModel = pXmlCollisionModel->FirstChildElement("collisionModel");

    pXmlPlane = NULL;
    for(int i = 0; i < groundCollisionModelCount; ++i)
    {
        Ptr<CollisionModel> cm = *new CollisionModel();
        int planeCount = 0;
        pXmlCollisionModel->QueryIntAttribute("planeCount", &planeCount);

        pXmlPlane = pXmlCollisionModel->FirstChildElement("plane");
        for(int j = 0; j < planeCount; ++j)
        {
            Vector3f norm;
            pXmlPlane->QueryFloatAttribute("nx", &norm.x);
            pXmlPlane->QueryFloatAttribute("ny", &norm.y);
            pXmlPlane->QueryFloatAttribute("nz", &norm.z);
                float D = 0.f;
            pXmlPlane->QueryFloatAttribute("d", &D);
            Planef p(norm.z, norm.y, norm.x * -1.0f, D);
            cm->Add(p);
            pXmlPlane = pXmlPlane->NextSiblingElement("plane");
        }

        if (pGroundCollisions)
        pGroundCollisions->push_back(cm);
        pXmlCollisionModel = pXmlCollisionModel->NextSiblingElement("collisionModel");
    }
    }
    WriteLog("Done.");
	return true;
}

void XmlHandler::ParseVectorString(const char* str, std::vector<OVR::Vector3f> *array,
	                               bool is2element)
{
    size_t stride = is2element ? 2 : 3;
    size_t stringLength = strlen(str);
    size_t element = 0;
    float v[3];

    for(size_t j = 0; j < stringLength;)
    {
        size_t k = j + 1;
        for(; k < stringLength; ++k)
        {
            if(str[k] == ' ')
            {
                break;
            }
        }
        char text[20];
        for(size_t l = 0; l < k - j; ++l)
        {
            text[l] = str[j + l];
        }
        text[k - j] = '\0';
        v[element] = (float)atof(text);

        if(element == (stride - 1))
        {
            //we've got all the elements of our vertex, so store them
            OVR::Vector3f vect;
            vect.x = v[0];
            vect.y = v[1];
            vect.z = is2element ? 0.0f : v[2];
            array->push_back(vect);
        }

        j = k + 1;
        element = (element + 1) % stride;
    }
}

}} // OVR::Render

#ifdef OVR_DEFINE_NEW
#define new OVR_DEFINE_NEW
#endif
