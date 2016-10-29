/************************************************************************************
Filename    :   Win32_CameraCone.h
Content     :   Shared functionality for the tracker's cone
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
#ifndef OVR_Win32_CameraCone_h
#define OVR_Win32_CameraCone_h

struct CameraCone
{
    XMFLOAT3 v[8];    // Corners of the cone
    Model *SolidModel;
    Model *WireModel;

    //------------------------------------------------------------------------------------
    CameraCone(BasicVR * pbasicVR)
    {
        // Handle the simple case of a single tracker. 
        // Find the corners of the camera cone, from SDK description          // v4-------v5
        float hFOV  = pbasicVR->TrackerDescArray[0].FrustumHFovInRadians;     // | \     / |
        float vFOV  = pbasicVR->TrackerDescArray[0].FrustumVFovInRadians;     // |  v0-v1  |
        float nearZ = pbasicVR->TrackerDescArray[0].FrustumNearZInMeters;     // |  | C |  |
        float farZ  = pbasicVR->TrackerDescArray[0].FrustumFarZInMeters;      // |  v2-v3  |
                                                                              // | /     \ |
                                                                              // v6-------v7
        XMFLOAT3 baseVec3(tan(0.5f * hFOV), tan(0.5f * vFOV), 1.0f);
        v[0] = v[4] = XMFLOAT3( baseVec3.x, -baseVec3.y, 1.0f);
        v[1] = v[5] = XMFLOAT3(-baseVec3.x, -baseVec3.y, 1.0f);
        v[2] = v[6] = XMFLOAT3( baseVec3.x,  baseVec3.y, 1.0f);
        v[3] = v[7] = XMFLOAT3(-baseVec3.x,  baseVec3.y, 1.0f);

        // Project to near and far planes
        for (int i = 0; i < 8; i++)
        {
            float depth = (i < 4 ? nearZ : farZ);
            v[i].x *= depth;
            v[i].y *= depth;
            v[i].z *= depth;
        }

        // Model of wireframe camera and edges
        TriangleSet ts1;
        DWORD color = 0xffffffff;
        float boxRadius = 0.02f;
        ts1.AddSolidColorBox(-boxRadius,-boxRadius,-boxRadius,boxRadius,boxRadius,boxRadius,color);

        #define AddEdge(i0,i1) ts1.AddQuad(Vertex(v[i0],color,0,0),Vertex(v[i1],color,0,0), \
                                           Vertex(v[i1],color,0,0),Vertex(v[i1],color,0,0));

        AddEdge(0,1); AddEdge(1,3); AddEdge(3,2); AddEdge(2,0);
        AddEdge(4,5); AddEdge(5,7); AddEdge(7,6); AddEdge(6,4);
        AddEdge(4,0); AddEdge(5,1); AddEdge(7,3); AddEdge(6,2);
        Texture  * tex1 = new Texture(false,256,256,Texture::AUTO_WHITE);
        Material * mat1 = new Material(tex1, Material::MAT_WRAP | Material::MAT_WIRE | Material::MAT_ZALWAYS | Material::MAT_NOCULL | Material::MAT_TRANS);
        WireModel = new Model(&ts1, XMFLOAT3(0, 0, 0), XMFLOAT4(0,0,0,1), mat1);

        // Model of solid planes
        TriangleSet ts2;
        float gridDensity = 6.0f;
        #define AddPlane(i0,i1,i2,i3,U,V) ts2.AddQuad(Vertex(v[i0],color,gridDensity*v[i0].U,gridDensity*v[i0].V), \
                                                      Vertex(v[i1],color,gridDensity*v[i1].U,gridDensity*v[i1].V), \
                                                      Vertex(v[i2],color,gridDensity*v[i2].U,gridDensity*v[i2].V), \
                                                      Vertex(v[i3],color,gridDensity*v[i3].U,gridDensity*v[i3].V))
        AddPlane(4,0,6,2,z,y); // Left
        AddPlane(1,5,3,7,z,y); // Right
        AddPlane(4,5,0,1,x,z); // Top
        AddPlane(2,3,6,7,x,z); // Bot
        AddPlane(5,4,7,6,x,y); // Back
        SolidModel = new Model(&ts2, XMFLOAT3(0, 0, 0), XMFLOAT4(0, 0, 0, 1),
                        new Material(
                            new Texture(false,256,256,Texture::AUTO_GRID)
                        )
                    );
    }

    ~CameraCone()
    {
        delete SolidModel;
        delete WireModel;
    }

    //-----------------------------------------------------------
    float DistToPlane(XMVECTOR * p, XMVECTOR * p0, XMVECTOR * p1, XMVECTOR * p2)
    {
        XMVECTOR q0 = XMVectorSubtract(*p1, *p0);
        XMVECTOR q1 = XMVectorSubtract(*p2, *p0);
        XMVECTOR c = XMVector3Normalize(XMVector3Cross(q0, q1));
        XMVECTOR q = XMVectorSubtract(*p, *p0);
        return(XMVectorGetX(XMVector3Dot(c, q)));
    }

    //-----------------------------------------------------------
    float DistToPlane(XMVECTOR * p, XMFLOAT3 * pp0, XMFLOAT3 * pp1, XMFLOAT3 * pp2)
    {
        XMVECTOR p0 = XMVectorSet(pp0->x, pp0->y, pp0->z, 0);
        XMVECTOR p1 = XMVectorSet(pp1->x, pp1->y, pp1->z, 0);
        XMVECTOR p2 = XMVectorSet(pp2->x, pp2->y, pp2->z, 0);
        return(DistToPlane(p, &p0, &p1, &p2));
    }

    //-----------------------------------------------------------
    float DistToBoundary(XMVECTOR centreEyePosePos, ovrPosef cameraPose)
    {
        // Translate test point back
        centreEyePosePos = XMVectorSubtract(centreEyePosePos, ConvertToXM(cameraPose.Position));
        // Rotate test point back
        centreEyePosePos = XMVector3Rotate(centreEyePosePos, XMQuaternionInverse(ConvertToXM(cameraPose.Orientation)));

        float dist = DistToPlane(&centreEyePosePos, &v[0], &v[3], &v[1]); // Front
        dist = min(dist, DistToPlane(&centreEyePosePos, &v[5], &v[6], &v[4]));// Back
        dist = min(dist, DistToPlane(&centreEyePosePos, &v[4], &v[2], &v[0]));// Left
        dist = min(dist, DistToPlane(&centreEyePosePos, &v[1], &v[7], &v[5]));// Right
        dist = min(dist, DistToPlane(&centreEyePosePos, &v[4], &v[1], &v[5]));// Top
        dist = min(dist, DistToPlane(&centreEyePosePos, &v[2], &v[7], &v[3]));// Bottom
        return(dist);
    }

    //-----------------------------------------------------------
    void RenderToEyeBuffer(VRLayer * vrLayer, int eye, ovrTrackingState * pTrackingState, ovrTrackerPose * pTrackerPose, float proportionVisible)
    {
        // Update pose of the models
        // Sure we do this twice, but keeps the code cleaner
        WireModel->Rot = SolidModel->Rot = ConvertToXMF(pTrackerPose->Pose.Orientation);
        WireModel->Pos = SolidModel->Pos = ConvertToXMF(pTrackerPose->Pose.Position);
        bool tracked = pTrackingState->StatusFlags & ovrStatus_PositionTracked ? true : false;

        // Now render camera volume, using its own static 'zero' camera, so purely rift components
        Camera finalCam(ConvertToXM(vrLayer->EyeRenderPose[eye].Position),
                        ConvertToXM(vrLayer->EyeRenderPose[eye].Orientation));
        XMMATRIX view = finalCam.GetViewMatrix();
        ovrMatrix4f p = ovrMatrix4f_Projection(vrLayer->EyeRenderDesc[eye].Fov, 0.01f/*0.2f*/, 1000.0f, ovrProjection_None);
        XMMATRIX proj = XMMatrixSet(p.M[0][0], p.M[1][0], p.M[2][0], p.M[3][0],
            p.M[0][1], p.M[1][1], p.M[2][1], p.M[3][1],
            p.M[0][2], p.M[1][2], p.M[2][2], p.M[3][2],
            p.M[0][3], p.M[1][3], p.M[2][3], p.M[3][3]);
        XMMATRIX prod = XMMatrixMultiply(view, proj);

        // Render two components of camera matrix, wireframe last to sort on top
        // and color red if not tracked
        SolidModel->Render(&prod, 1, tracked ? 1.0f : 0.0f, tracked ? 1.0f : 0.0f, proportionVisible, true);
        WireModel->Render(&prod, 1, tracked ? 1.0f : 0.0f, tracked ? 1.0f : 0.0f, proportionVisible, true);
    }

};

#endif // OVR_Win32_CameraCone_h
