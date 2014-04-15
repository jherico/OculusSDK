/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_SensorFilter.cpp
Content     :   Basic filtering of sensor this->Data
Created     :   March 7, 2013
Authors     :   Steve LaValle, Anna Yershova, Max Katsev

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

#include "OVR_SensorFilter.h"

namespace OVR {

template <typename T>
Vector3<T> SensorFilter<T>::Median() const
{
    Vector3<T> result;
    T* slice = (T*) OVR_ALLOC(this->ElemCount * sizeof(T));

    for (int coord = 0; coord < 3; coord++)
    {
        for (int i = 0; i < this->ElemCount; i++)
            slice[i] = this->Data[i][coord];
        result[coord] = Alg::Median(ArrayAdaptor(slice, this->ElemCount));
    }

    OVR_FREE(slice);
    return result;
}

//  Only the diagonal of the covariance matrix.
template <typename T>
Vector3<T> SensorFilter<T>::Variance() const
{
    Vector3<T> mean = this->Mean();
    Vector3<T> total;
    for (int i = 0; i < this->ElemCount; i++) 
    {
        total.x += (this->Data[i].x - mean.x) * (this->Data[i].x - mean.x);
        total.y += (this->Data[i].y - mean.y) * (this->Data[i].y - mean.y);
        total.z += (this->Data[i].z - mean.z) * (this->Data[i].z - mean.z);
    }
    return total / (float) this->ElemCount;
}

template <typename T>
Matrix3<T> SensorFilter<T>::Covariance() const
{
    Vector3<T> mean = this->Mean();
    Matrix3<T> total;
    for (int i = 0; i < this->ElemCount; i++) 
    {
        total.M[0][0] += (this->Data[i].x - mean.x) * (this->Data[i].x - mean.x);
        total.M[1][0] += (this->Data[i].y - mean.y) * (this->Data[i].x - mean.x);
        total.M[2][0] += (this->Data[i].z - mean.z) * (this->Data[i].x - mean.x);
        total.M[1][1] += (this->Data[i].y - mean.y) * (this->Data[i].y - mean.y);
        total.M[2][1] += (this->Data[i].z - mean.z) * (this->Data[i].y - mean.y);
        total.M[2][2] += (this->Data[i].z - mean.z) * (this->Data[i].z - mean.z);
    }
    total.M[0][1] = total.M[1][0];
    total.M[0][2] = total.M[2][0];
    total.M[1][2] = total.M[2][1];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            total.M[i][j] /= (float) this->ElemCount;
    return total;
}

template <typename T>
Vector3<T> SensorFilter<T>::PearsonCoefficient() const
{
    Matrix3<T> cov = this->Covariance();
    Vector3<T> pearson;
    pearson.x = cov.M[0][1]/(sqrt(cov.M[0][0])*sqrt(cov.M[1][1]));
    pearson.y = cov.M[1][2]/(sqrt(cov.M[1][1])*sqrt(cov.M[2][2]));
    pearson.z = cov.M[2][0]/(sqrt(cov.M[2][2])*sqrt(cov.M[0][0]));

    return pearson;
}

} //namespace OVR
