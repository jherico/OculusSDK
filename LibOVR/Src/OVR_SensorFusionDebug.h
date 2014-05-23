/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_SensorFusionDebug.h
Content     :   Friend proxy to allow debugging access to SensorFusion
Created     :   April 16, 2014
Authors     :   Dan Gierl

Copyright   :   Copyright 2014 Oculus VR, Inc. All Rights reserved.

Licensed under the Oculus VR Rift SDK License Version 3.1 (the "License"); 
you may not use the Oculus VR Rift SDK except in compliance with the License, 
which is provided at the time of installation or download, or which 
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at

http://www.oculusvr.com/licenses/LICENSE-3.1 

Unless required by applicable law or agreed to in writing, the Oculus VR SDK 
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*************************************************************************************/

#ifndef OVR_SensorFusionDebug_h
#define OVR_SensorFusionDebug_h

#include "OVR_SensorFusion.h"

namespace OVR {

class SensorFusionDebug
{
private:

	SensorFusion *	sf;

public:

	SensorFusionDebug (SensorFusion * const sf) :
		sf(sf)
	{
	}
	
	// Returns the number of magnetometer reference points currently gathered
	int			GetNumMagRefPoints		() const;
	// Returns the index of the magnetometer reference point being currently used
	int			GetCurMagRefPointIdx	() const;
	// Returns a copy of all the data associated with a magnetometer reference point
	// This includes it's score, the magnetometer reading as a vector,
	// and the HMD's pose at the time it was gathered
	void		GetMagRefData			(int idx, int * score, Vector3d * magBF, Quatd * magPose) const;

};

//------------------------------------------------------------------------------------
// Magnetometer reference point access functions

int SensorFusionDebug::GetNumMagRefPoints() const
{
	return (int)sf->MagRefs.GetSize();
}

int SensorFusionDebug::GetCurMagRefPointIdx() const
{
	return sf->MagRefIdx;
}

void SensorFusionDebug::GetMagRefData(int idx, int * score, Vector3d * magBF, Quatd * magPose) const
{
	OVR_ASSERT(idx >= 0 && idx < GetNumMagRefPoints());
	*score = sf->MagRefs[idx].Score;
	*magBF = sf->MagRefs[idx].InImuFrame;
	*magPose = sf->MagRefs[idx].WorldFromImu.Rotation;
}

} // OVR

#endif