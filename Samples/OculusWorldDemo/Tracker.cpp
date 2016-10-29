/************************************************************************************
Created     :   Dec 12, 2015
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
/// This file contains visualization code for the tracking sensor and cone.
/// Shortcut key for activating is 'T'.
/// Or use the TAB menu, under 'Tracking' and 'Visualize Cone'

#include "OculusWorldDemo.h"

#include <algorithm>

// If set, pyramid sides are filled in with a pattern; else the image
// is minimized to just have lines.
bool    drawWalls = false;

// Some choices/options
bool    trackerLinesAlwaysVisible = true;
bool    frontOfGridAsWell = false;
bool    extendLinesToTracker = false;
float   minimumAlphaOfTracker = 0.00f; // 0.01f
Color   baseColourOfTracker = Color(255, 255, 255, 255); // Modulated by tint. Color(0, 0, 0, 255) forces black.

// Measurements used
float radiusOfTrackerHead = 0.0165f;
float lengthOfTrackerHead = 0.073f;
float heightOfStalk = 0.23f;
float radiusOfStalk = 0.004f;
float radiusOfStand = 0.055f;
float heightOfStand = 0.005f;
float excessiveHeightOfStalk = 1.00f; // We scale this to get the right height.


//-----------------------------------------------------------------------
void LOCAL_AddCylinderVerts(Model* m, Vector3f centre, float radius, float height, float segments, Color c, bool swapYandZ)
{
	for (int i = 0; i < segments; i++)
	{
		float angleGap = (360.0f / segments) / 57.2957795f;
		float angle0 = i*angleGap;
		float angle1 = (i + 1) * angleGap;

		Vector3f v0, v1, v2, v3, centre0, centre1;

		if (swapYandZ)
		{
			centre0 = Vector3f(0, 0, 0.5f*height);
			centre1 = Vector3f(0, 0, -0.5f*height);
			v0 = Vector3f(radius * sin(angle0), radius * cos(angle0), +0.5f*height);
			v1 = Vector3f(radius * sin(angle1), radius * cos(angle1), + 0.5f*height );
			v2 = Vector3f(radius * sin(angle0), radius * cos(angle0), - 0.5f*height );
			v3 = Vector3f(radius * sin(angle1), radius * cos(angle1), - 0.5f*height);
		}
		else
		{
			centre0 = Vector3f(0, 0.5f*height, 0);
			centre1 = Vector3f(0, -0.5f*height, 0);
			v0 = Vector3f(radius * sin(angle0), +0.5f*height, radius * cos(angle0));
			v1 = Vector3f(radius * sin(angle1), +0.5f*height, radius * cos(angle1));
			v2 = Vector3f(radius * sin(angle0), -0.5f*height, radius * cos(angle0));
			v3 = Vector3f(radius * sin(angle1), -0.5f*height, radius * cos(angle1));
		}

		centre0 += centre;
		centre1 += centre;
		v0 += centre;
		v1 += centre;
		v2 += centre;
		v3 += centre;

		m->AddQuad(Vertex(v0, c, 0, 0), Vertex(v1, c, 1, 0), Vertex(v2, c, 0, 1), Vertex(v3, c, 1, 1)); // side
		m->AddQuad(Vertex(v2, c, 0, 0), Vertex(v3, c, 1, 0), Vertex(centre1, c, 0, 1), Vertex(centre1, c, 0, 1)); // top
		m->AddQuad(Vertex(v0, c, 0, 0), Vertex(centre0, c, 0, 1), Vertex(v1, c, 1, 0), Vertex(v1, c, 1, 0));  // bottom
	}
}

//-----------------------------------------------------------
float LOCAL_DistToPlane(Vector3f * p, Vector3f * p0, Vector3f * p1, Vector3f * p2)
{
	Vector3f q0 = *p1 - *p0;
	Vector3f q1 = *p2 - *p0;
	Vector3f c = (q0.Cross(q1)).Normalized();
	Vector3f q = *p - *p0;
	return(c.Dot(q));
}

//-----------------------------------------------------------
float Tracker::DistToBoundary(Vector3f centreEyePosePos, ovrPosef cameraPose, bool includeTopAndBottom)
{
	// Translate test point back
	centreEyePosePos = centreEyePosePos - cameraPose.Position;
	// Rotate test point back
	centreEyePosePos = (((Quatf)(cameraPose.Orientation)).Inverse()).Rotate(centreEyePosePos);

	float dist = LOCAL_DistToPlane(&centreEyePosePos, &v[0], &v[3], &v[1]); // Front
	dist = OVRMath_Min(dist, LOCAL_DistToPlane(&centreEyePosePos, &v[5], &v[6], &v[4]));// Back
	dist = OVRMath_Min(dist, LOCAL_DistToPlane(&centreEyePosePos, &v[4], &v[2], &v[0]));// Left
	dist = OVRMath_Min(dist, LOCAL_DistToPlane(&centreEyePosePos, &v[1], &v[7], &v[5]));// Right
	if (includeTopAndBottom) dist = OVRMath_Min(dist, LOCAL_DistToPlane(&centreEyePosePos, &v[4], &v[1], &v[5]));// Top
	if (includeTopAndBottom) dist = OVRMath_Min(dist, LOCAL_DistToPlane(&centreEyePosePos, &v[2], &v[7], &v[3]));// Bottom
	return(dist);
}


//---------------------------------------------------------------
void Tracker::AddTrackerConeVerts(ovrSession Session, Model* m, bool isItEdges)
{
	//Get attributes of camera cone
	std::vector<ovrTrackerDesc>   TrackerDescArray;
	unsigned int trackerCount = std::max<unsigned int>(1, ovr_GetTrackerCount(Session)); // Make sure there's always at least one.
	for (unsigned int i = 0; i < trackerCount; ++i)
		TrackerDescArray.push_back(ovr_GetTrackerDesc(Session, i));
	                                                            // v4-------v5
	float hFOV = TrackerDescArray[0].FrustumHFovInRadians;      // | \     / |
	float vFOV  = TrackerDescArray[0].FrustumVFovInRadians;     // |  v0-v1  |
	float nearZ = TrackerDescArray[0].FrustumNearZInMeters;     // |  | C |  |
	float farZ  = TrackerDescArray[0].FrustumFarZInMeters;      // |  v2-v3  |
	                                                            // | /     \ |
	                                                            // v6-------v7
    // MA: Having the lines/pyramid start closer to camera looks better.
    nearZ = 0.08f;

	Vector3f baseVec3(tan(0.5f * hFOV), tan(0.5f * vFOV), 1.0f);
	v[0] = v[4] = Vector3f(baseVec3.x, -baseVec3.y, 1.0f);
	v[1] = v[5] = Vector3f(-baseVec3.x, -baseVec3.y, 1.0f);
	v[2] = v[6] = Vector3f(baseVec3.x, baseVec3.y, 1.0f);
	v[3] = v[7] = Vector3f(-baseVec3.x, baseVec3.y, 1.0f);
	v[8] = Vector3f(0, 0, 0.5f*lengthOfTrackerHead); //front of tracker location

	// Project to near and far planes
	for (int i = 0; i < 8; i++)
	{
		float depth = (i < 4 ? nearZ : farZ);
		v[i].x *= depth;
		v[i].y *= depth;
		v[i].z *= depth;
	}

	Color c = Color(255, 255, 255, 255);

	if (isItEdges) //Wire parts
	{
		#define AddEdge(i0,i1) m->AddQuad(    \
		Vertex(v[i0],c,0,0),Vertex(v[i1],c,0,0), \
		Vertex(v[i1], c, 0, 0), Vertex(v[i1], c, 0, 0));

        if (drawWalls)
        {   // Add wireframe fronty and back outlines if we have walls
            AddEdge(0, 1); AddEdge(1, 3); AddEdge(3, 2); AddEdge(2, 0);
            AddEdge(4, 5); AddEdge(5, 7); AddEdge(7, 6); AddEdge(6, 4);
        }
		AddEdge(4, 0); AddEdge(5, 1); AddEdge(7, 3); AddEdge(6, 2);
		if (extendLinesToTracker)
		{
			AddEdge(8, 0);	AddEdge(8, 1);	AddEdge(8, 2);	AddEdge(8, 3);
		}
	}
	else //Solid planes
	{
		float gridDensity = 6.0f;
		#define AddPlane(i0,i1,i2,i3,U,V,dense) m->AddQuad(    \
		Vertex(v[i0], c, dense*v[i0].U, dense*v[i0].V), \
		Vertex(v[i1], c, dense*v[i1].U, dense*v[i1].V), \
		Vertex(v[i2], c, dense*v[i2].U, dense*v[i2].V), \
		Vertex(v[i3], c, dense*v[i3].U, dense*v[i3].V))
		AddPlane(4, 0, 6, 2, z, y, gridDensity); // Left
		AddPlane(1, 5, 3, 7, z, y, gridDensity); // Right
		AddPlane(4, 5, 0, 1, x, z, gridDensity); // Top
		AddPlane(2, 3, 6, 7, x, z, gridDensity); // Bot
		AddPlane(5, 4, 7, 6, x, y, gridDensity); // Back
		if (frontOfGridAsWell) { AddPlane(0, 1, 2, 3, x, y, gridDensity); } // Front
	}
}


//------------------------------------------------------
void LOCAL_RenderModelWithAlpha(RenderDevice* pRender, Model * m, Matrix4f mat)
{
	// Need to call the regular function if needed to set up
	if (!m->VertexBuffer) pRender->Render(Matrix4f(), m);
	pRender->RenderWithAlpha(m->Fill, m->VertexBuffer, m->IndexBuffer,
		mat * m->GetMatrix(), -1/*to trigger normal alpha blend*/, (unsigned)m->Indices.size(),m->GetPrimType());
}


//----------------------------------------------------------------
void Tracker::Init(ovrSession Session, std::string mainFilePathNoExtension, RenderDevice* pRender, bool SrgbRequested, bool AnisotropicSample)
{
	// Load textures - just have to call everything Tuscany_, and put in assets file.
	int textureLoadFlags = 0;
	textureLoadFlags |= SrgbRequested ? TextureLoad_SrgbAware : 0;
	textureLoadFlags |= AnisotropicSample ? TextureLoad_Anisotropic : 0;
	textureLoadFlags |= TextureLoad_MakePremultAlpha;

	Ptr<File>	 whiteFile = *new SysFile((mainFilePathNoExtension + "_White.dds").c_str());
	Ptr<File>	 gridFile = *new SysFile((mainFilePathNoExtension + "_Grid.dds").c_str()); 
	Ptr<Texture> whiteTexture = *LoadTextureDDSTopDown(pRender, whiteFile, textureLoadFlags);
	Ptr<Texture> gridTexture = *LoadTextureDDSTopDown(pRender, gridFile, textureLoadFlags);
	whiteTexture->SetSampleMode(Sample_Anisotropic | Sample_Repeat);
	gridTexture->SetSampleMode(Sample_Anisotropic | Sample_Repeat);

	// Make materials
	Ptr<ShaderFill> whiteFill = *new ShaderFill(*pRender->CreateShaderSet());
	whiteFill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Vertex, VShader_MVP));
	whiteFill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Fragment, FShader_TextureNoClip));
	whiteFill->SetTexture(0, whiteTexture);

	Ptr<ShaderFill> gridFill = *new ShaderFill(*pRender->CreateShaderSet());
	gridFill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Vertex, VShader_MVP));
	gridFill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Fragment, FShader_TextureNoClip));
	gridFill->SetTexture(0, gridTexture);

	Ptr<ShaderFill> wireFill = *new ShaderFill(*pRender->CreateShaderSet());
	wireFill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Vertex, VShader_MVP));
	wireFill->GetShaders()->SetShader(pRender->LoadBuiltinShader(Shader_Fragment, FShader_TextureNoClip));
	wireFill->SetTexture(0, whiteTexture);

	// Make models
	TrackerHeadModel = *new Model(Prim_Lines);
	TrackerHeadModel->Fill = wireFill;
	LOCAL_AddCylinderVerts(TrackerHeadModel, Vector3f(0, 0, 0), radiusOfTrackerHead, lengthOfTrackerHead, 30,
                           baseColourOfTracker, true);

	TrackerStalkModel = *new Model(Prim_Lines);
	TrackerStalkModel->Fill = wireFill;
	LOCAL_AddCylinderVerts(TrackerStalkModel, Vector3f(0, -0.5f*excessiveHeightOfStalk, 0), radiusOfStalk,
		excessiveHeightOfStalk, 20, baseColourOfTracker, false);

	TrackerStandModel = *new Model(Prim_Lines);
	TrackerStandModel->Fill = wireFill;
	LOCAL_AddCylinderVerts(TrackerStandModel, Vector3f(0, 0, 0), radiusOfStand,
		heightOfStand, 30, baseColourOfTracker, false);

	TrackerConeModel = *new Model(Prim_Triangles);
	TrackerConeModel->Fill = gridFill;
	AddTrackerConeVerts(Session, TrackerConeModel, false);

	TrackerLinesModel = *new Model(Prim_Lines);
	TrackerLinesModel->Fill = wireFill;
	AddTrackerConeVerts(Session, TrackerLinesModel, true);
}

//----------------------------------------------------------------
void Tracker::Clear(void)
{
	TrackerHeadModel.Clear();
	TrackerStalkModel.Clear();
	TrackerStandModel.Clear();
	TrackerConeModel.Clear();
	TrackerLinesModel.Clear();
}


//----------------------------------------------------------------------
void Tracker::Draw(ovrSession Session, RenderDevice* pRender, Player ThePlayer, ovrTrackingOrigin TrackingOriginType,
	               bool Sitting, float ExtraSittingAltitude, Matrix4f * /*ViewFromWorld*/, int eye, ovrPosef * EyeRenderPose)
{
    OVR_UNUSED2(ExtraSittingAltitude, Sitting);

	// Don't render if not ready
	if (!TrackerHeadModel) return;

	// Initial rendering setup
	pRender->SetDepthMode(true, true);
	pRender->SetCullMode(OVR::Render::D3D11::RenderDevice::Cull_Off);

	// Draw in local frame of reference, so get view matrix
	Quatf eyeRot = EyeRenderPose[eye].Orientation;
	Vector3f up = eyeRot.Rotate(UpVector);
	Vector3f forward = eyeRot.Rotate(ForwardVector);
	Vector3f viewPos = EyeRenderPose[eye].Position;
	Matrix4f localViewMat = Matrix4f::LookAtRH(viewPos, viewPos + forward, up);

	// Get some useful values about the situation
	Vector3f          headWorldPos  = ThePlayer.GetHeadPosition(TrackingOriginType);
	ovrTrackerPose    trackerPose   = ovr_GetTrackerPose(Session, 0);
	Vector3f          centreEyePos  = ((Vector3f)(EyeRenderPose[0].Position) + (Vector3f)(EyeRenderPose[1].Position))*0.5f;
	double            ftiming       = ovr_GetPredictedDisplayTime(Session, 0);
	ovrTrackingState  trackingState = ovr_GetTrackingState(Session, ftiming, ovrTrue);
	bool              tracked       = trackingState.StatusFlags & ovrStatus_PositionTracked ? true : false;

	// Find altitude of stand.
    // If we are at floor level, display the tracker stand on the physical floor.
    // If are using eye level coordinate system, just render the standard height of the stalk.
    float altitudeOfFloorInLocalSpace;
    if (TrackingOriginType == ovrTrackingOrigin_FloorLevel)
        altitudeOfFloorInLocalSpace = 0.01f;
    else
        altitudeOfFloorInLocalSpace = trackerPose.Pose.Position.y - 0.22f;  //0.18f;

	Vector3f localStandPos = Vector3f(trackerPose.Pose.Position.x, altitudeOfFloorInLocalSpace,
                                      trackerPose.Pose.Position.z);

	// Set position of tracker models according to pose.
	TrackerHeadModel->SetPosition(trackerPose.Pose.Position);
	TrackerHeadModel->SetOrientation(trackerPose.Pose.Orientation);
	
    // We scale the stalk so that it has correct physical height.
    Matrix4f stalkScale = Matrix4f::Scaling(1.0f, trackerPose.Pose.Position.y - altitudeOfFloorInLocalSpace - 0.0135f, 1.0f);
    TrackerStalkModel->SetMatrix(Matrix4f::Translation(Vector3f(trackerPose.Pose.Position) - Vector3f(0,0.0135f,0)) * stalkScale *
                                 Matrix4f(TrackerStalkModel->GetOrientation()));
	
    TrackerStandModel->SetPosition(localStandPos);
	TrackerConeModel->SetPosition(trackerPose.Pose.Position);
	TrackerConeModel->SetOrientation(trackerPose.Pose.Orientation);
	TrackerLinesModel->SetPosition(trackerPose.Pose.Position);
	TrackerLinesModel->SetOrientation(trackerPose.Pose.Orientation);


    if (trackerLinesAlwaysVisible)
        pRender->SetDepthMode(false, true);

	// Set rendering tint proportional to proximity, and red if not tracked. 
	float dist = DistToBoundary(centreEyePos, trackerPose.Pose, true);    
	 //OVR_DEBUG_LOG(("Dist = %0.3f\n", dist));
    
    // This defines a color ramp at specified distances from the edge.
    // Display staring at 0.4 - 0.2 meter [alpha 0->1]
    // Turn to yellow after [0.2]
    float       distThreshods[4]   = { 0.0f, 0.1f, 0.2f, 0.35f };
    Vector4f    thresholdColors[4] = {
        Vector4f(1.0f, 0.3f, 0.0f, 1.0f),   // Yellow-red
        Vector4f(1.0f, 1.0f, 0.0f, 0.8f),   // Yellow
        Vector4f(1.0f, 1.0f, 1.0f, 0.6f),   // White
        Vector4f(1.0f, 1.0f, 1.0f, 0.0f)    // White-transparent
    };

    // Assign tint based on the lookup table
    Vector4f globalTint = Vector4f(1, 1, 1, 0);

    int distSearch = 0;
    if (dist <= 0.0f)
        dist = 0.001f;
    for (; distSearch < sizeof(distThreshods) / sizeof(distThreshods[0]) - 1; distSearch++)
    {
        if (dist < distThreshods[distSearch+1])
        {
            float startT = distThreshods[distSearch];
            float endT   = distThreshods[distSearch+1];
            float factor = (dist - startT) / (endT - startT);

            globalTint = thresholdColors[distSearch] * (1.0f - factor) +
                         thresholdColors[distSearch + 1] * factor;
            break;
        }
    }
    
    if (!tracked)
        globalTint = Vector4f(1, 0, 0, 1);
    
    pRender->SetGlobalTint(globalTint);

    if (minimumAlphaOfTracker > globalTint.w)
        globalTint.w = minimumAlphaOfTracker;

    // We try to draw twice here: Once with Z clipping to give a bright image,
    // and once with Z testing off to give a dim outline for those cases.

    // Solid bakground
    if (globalTint.w > 0.01)
    {
        pRender->SetDepthMode(true, true);

        // Draw the tracker representation
        LOCAL_RenderModelWithAlpha(pRender, TrackerStandModel, localViewMat);
        LOCAL_RenderModelWithAlpha(pRender, TrackerStalkModel, localViewMat);
        LOCAL_RenderModelWithAlpha(pRender, TrackerHeadModel, localViewMat);
        LOCAL_RenderModelWithAlpha(pRender, TrackerLinesModel, localViewMat);
        if (drawWalls)
            LOCAL_RenderModelWithAlpha(pRender, TrackerConeModel, localViewMat);
    }

    
    if (globalTint.w > 0.01f)
        globalTint.w = 0.01f;    
    pRender->SetGlobalTint(globalTint);
    pRender->SetDepthMode(false, true);
    LOCAL_RenderModelWithAlpha(pRender, TrackerStandModel, localViewMat);
    LOCAL_RenderModelWithAlpha(pRender, TrackerStalkModel, localViewMat);
    LOCAL_RenderModelWithAlpha(pRender, TrackerHeadModel, localViewMat);
    LOCAL_RenderModelWithAlpha(pRender, TrackerLinesModel, localViewMat);
    if (drawWalls)
        LOCAL_RenderModelWithAlpha(pRender, TrackerConeModel, localViewMat);

	// Revert to rendering defaults
	pRender->SetGlobalTint(Vector4f(1, 1, 1, 1));
	pRender->SetCullMode(RenderDevice::Cull_Back);
	pRender->SetDepthMode(true, true);
}
