/************************************************************************************

Filename    :   OVR_Math.h
Content     :   Implementation of 3D primitives such as vectors, matrices.
Created     :   September 4, 2012
Authors     :   Andrew Reisse, Michael Antonov, Anna Yershova

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

#include "OVR_Math.h"
#include "OVR_Log.h"

#include <float.h>


namespace OVR {


//-------------------------------------------------------------------------------------
// ***** Math


// Single-precision Math constants class.
const float Math<float>::Pi      = 3.1415926f;
const float Math<float>::TwoPi   = 3.1415926f * 2;
const float Math<float>::PiOver2 = 3.1415926f / 2.0f;
const float Math<float>::PiOver4 = 3.1415926f / 4.0f;
const float Math<float>::E       = 2.7182818f;

const float Math<float>::MaxValue			= FLT_MAX;
const float Math<float>::MinPositiveValue	= FLT_MIN;

const float Math<float>::RadToDegreeFactor	= 360.0f / Math<float>::TwoPi;
const float Math<float>::DegreeToRadFactor	= Math<float>::TwoPi / 360.0f;

const float Math<float>::Tolerance			= 0.00001f;
const float Math<float>::SingularityRadius	= 0.0000001f; // Use for Gimbal lock numerical problems

// Double-precision Math constants class.
const double Math<double>::Pi      = 3.14159265358979;
const double Math<double>::TwoPi   = 3.14159265358979 * 2;
const double Math<double>::PiOver2 = 3.14159265358979 / 2.0;
const double Math<double>::PiOver4 = 3.14159265358979 / 4.0;
const double Math<double>::E       = 2.71828182845905;

const double Math<double>::MaxValue				= DBL_MAX;
const double Math<double>::MinPositiveValue		= DBL_MIN;

const double Math<double>::RadToDegreeFactor	= 360.0 / Math<double>::TwoPi;
const double Math<double>::DegreeToRadFactor	= Math<double>::TwoPi / 360.0;

const double Math<double>::Tolerance			= 0.00001;
const double Math<double>::SingularityRadius	= 0.000000000001; // Use for Gimbal lock numerical problems



//-------------------------------------------------------------------------------------
// ***** Matrix4

template<>
const Matrix4<float> Matrix4<float>::IdentityValue = Matrix4<float>(1.0f, 0.0f, 0.0f, 0.0f, 
                                                                    0.0f, 1.0f, 0.0f, 0.0f, 
                                                                    0.0f, 0.0f, 1.0f, 0.0f,
                                                                    0.0f, 0.0f, 0.0f, 1.0f);

template<>
const Matrix4<double> Matrix4<double>::IdentityValue = Matrix4<double>(1.0, 0.0, 0.0, 0.0, 
                                                                       0.0, 1.0, 0.0, 0.0, 
                                                                       0.0, 0.0, 1.0, 0.0,
                                                                       0.0, 0.0, 0.0, 1.0);



} // Namespace OVR
