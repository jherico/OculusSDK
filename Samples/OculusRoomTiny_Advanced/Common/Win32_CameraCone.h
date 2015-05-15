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
    Vector3f v[8];    // Corners of the cone
    Model * SolidModel;
    Model * WireModel;

    //------------------------------------------------------------------------------------
    CameraCone(BasicVR * pbasicVR)
    {
        // Find the corners of the camera cone, from SDK description // v4-------v5
        float hFOV  = pbasicVR->HMD->CameraFrustumHFovInRadians;     // | \     / |
        float vFOV  = pbasicVR->HMD->CameraFrustumVFovInRadians;     // |  v0-v1  |
        float nearZ = pbasicVR->HMD->CameraFrustumNearZInMeters;     // |  | C |  |
        float farZ  = pbasicVR->HMD->CameraFrustumFarZInMeters;      // |  v2-v3  |
                                                                     // | /     \ |
        Matrix4f m;                                                  // v6-------v7
        v[0] = (m.RotationY(-0.5f*hFOV) * m.RotationX(-0.5f*vFOV)).Transform(Vector3f(0,0,nearZ));    
        v[1] = (m.RotationY(+0.5f*hFOV) * m.RotationX(-0.5f*vFOV)).Transform(Vector3f(0,0,nearZ));    
        v[2] = (m.RotationY(-0.5f*hFOV) * m.RotationX(+0.5f*vFOV)).Transform(Vector3f(0,0,nearZ));    
        v[3] = (m.RotationY(+0.5f*hFOV) * m.RotationX(+0.5f*vFOV)).Transform(Vector3f(0,0,nearZ));    
        v[4] = (m.RotationY(-0.5f*hFOV) * m.RotationX(-0.5f*vFOV)).Transform(Vector3f(0,0, farZ));    
        v[5] = (m.RotationY(+0.5f*hFOV) * m.RotationX(-0.5f*vFOV)).Transform(Vector3f(0,0, farZ));    
        v[6] = (m.RotationY(-0.5f*hFOV) * m.RotationX(+0.5f*vFOV)).Transform(Vector3f(0,0, farZ));    
        v[7] = (m.RotationY(+0.5f*hFOV) * m.RotationX(+0.5f*vFOV)).Transform(Vector3f(0,0, farZ));    
     
        // Model of wireframe camera and edges
        TriangleSet ts1;
        DWORD colour = 0xffffffff;
        float boxRadius = 0.02f; 
        ts1.AddSolidColorBox(-boxRadius,-boxRadius,-boxRadius,boxRadius,boxRadius,boxRadius,colour);
        #define AddEdge(i0,i1) ts1.AddQuad(Vertex(v[i0],colour,0,0),Vertex(v[i1],colour,0,0), \
                                           Vertex(v[i1],colour,0,0),Vertex(v[i1],colour,0,0)); 
        AddEdge(0,1);AddEdge(1,3);AddEdge(3,2);AddEdge(2,0);
        AddEdge(4,5);AddEdge(5,7);AddEdge(7,6);AddEdge(6,4);
        AddEdge(4,0);AddEdge(5,1);AddEdge(7,3);AddEdge(6,2);
        Texture  * tex1 = new Texture(false, Sizei(256,256),Texture::AUTO_WHITE);
        Material * mat1 = new Material(tex1,MAT_WRAP | MAT_WIRE | MAT_ZALWAYS | MAT_NOCULL);
        WireModel = new Model(&ts1,Vector3f(),mat1);

        // Model of solid planes
        TriangleSet ts2;
        float gridDensity = 6.0f;
        #define AddPlane(i0,i1,i2,i3,U,V) ts2.AddQuad(Vertex(v[i0],colour,gridDensity*v[i0].U,gridDensity*v[i0].V), \
                                                      Vertex(v[i1],colour,gridDensity*v[i1].U,gridDensity*v[i1].V), \
                                                      Vertex(v[i2],colour,gridDensity*v[i2].U,gridDensity*v[i2].V), \
                                                      Vertex(v[i3],colour,gridDensity*v[i3].U,gridDensity*v[i3].V))
        AddPlane(4,0,6,2,z,y); // Left
        AddPlane(1,5,3,7,z,y); // Right
        AddPlane(4,5,0,1,x,z); // Top
        AddPlane(2,3,6,7,x,z); // Bot
        AddPlane(5,4,7,6,x,y); // Back
        SolidModel = new Model(&ts2,Vector3f(0,0,0),
                     new Material(
                     new Texture(false,Sizei(256,256),Texture::AUTO_GRID)));
    }
    //-----------------------------------------------------------
    float DistToBoundary(Vector3f centreEyePosePos, ovrPosef cameraPose)
        {
        // Translate test point back
        centreEyePosePos -= cameraPose.Position;
        // Rotate test point back
        centreEyePosePos = (Matrix4f(cameraPose.Orientation).Inverted()).Transform(centreEyePosePos);

        #define DIST_TO_PLANE(p, p0, p1, p2)  ((((p1-p0).Cross(p2-p0)).Normalized()).Dot(p-p0))
        float dist =     DIST_TO_PLANE(centreEyePosePos,v[0],v[3],v[1]); // Front
        dist = min(dist, DIST_TO_PLANE(centreEyePosePos,v[5],v[6],v[4]));// Back
        dist = min(dist, DIST_TO_PLANE(centreEyePosePos,v[4],v[2],v[0]));// Left
        dist = min(dist, DIST_TO_PLANE(centreEyePosePos,v[1],v[7],v[5]));// Right
        dist = min(dist, DIST_TO_PLANE(centreEyePosePos,v[4],v[1],v[5]));// Top
        dist = min(dist, DIST_TO_PLANE(centreEyePosePos,v[2],v[7],v[3]));// Bottom
        return(dist);
        }

    //-----------------------------------------------------------
    void RenderToEyeBuffer(VRLayer * vrLayer, int eye, ovrTrackingState * pTrackingState, float proportionVisible)
    {
        // Update pose of the models
        // Sure we do this twice, but keeps the code cleaner
        WireModel->Rot = SolidModel->Rot = pTrackingState->CameraPose.Orientation;
        WireModel->Pos = SolidModel->Pos = pTrackingState->CameraPose.Position;
        bool tracked = pTrackingState->StatusFlags & ovrStatus_PositionTracked ? true : false;

        // Now render camera volume, using its own static 'zero' camera, so purely rift components
        Camera finalCam(vrLayer->EyeRenderPose[eye].Position,Matrix4f(vrLayer->EyeRenderPose[eye].Orientation));
        Matrix4f view = finalCam.GetViewMatrix();
        Matrix4f proj = ovrMatrix4f_Projection(vrLayer->EyeRenderDesc[eye].Fov, 0.01f/*0.2f*/, 1000.0f, ovrProjection_RightHanded);

        // Render two components of camera matrix, wireframe last to sort on top
        // and colour red if not tracked 
        SolidModel->Render(proj*view, 1, tracked ? 1.0f : 0.0f, tracked ? 1.0f : 0.0f, proportionVisible, true);
        WireModel->Render(proj*view, 1, tracked ? 1.0f : 0.0f, tracked ? 1.0f : 0.0f, proportionVisible, true);
    }

};

#endif // OVR_Win32_CameraCone_h
