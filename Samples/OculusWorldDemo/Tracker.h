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

#include <string>

class Tracker
{
private :
	Ptr<Model>			TrackerHeadModel;
	Ptr<Model>			TrackerStalkModel;
	Ptr<Model>			TrackerStandModel;
	Ptr<Model>			TrackerConeModel;
	Ptr<Model>			TrackerLinesModel;

	Vector3f v[9]; // Tracker cone verts, in 3D

public :

	void Init(ovrSession Session, std::string mainFilePathNoExtension, RenderDevice* pRender, bool SrgbRequested, bool AnisotropicSample);
	void Clear(void);
	void Draw(ovrSession Session, RenderDevice*       pRender, Player ThePlayer, ovrTrackingOrigin TrackingOriginType,
		bool Sitting, float ExtraSittingAltitude, Matrix4f * ViewFromWorld, int eye,ovrPosef * EyeRenderPose);
	float DistToBoundary(Vector3f centreEyePosePos, ovrPosef cameraPose, bool includeTopAndBottom);
	void AddTrackerConeVerts(ovrSession Session, Model* m, bool isItEdges);
};


