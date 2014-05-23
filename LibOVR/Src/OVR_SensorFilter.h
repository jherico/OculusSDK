/************************************************************************************

PublicHeader:   OVR.h
Filename    :   OVR_SensorFilter.h
Content     :   Basic filtering of sensor data
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

#ifndef OVR_SensorFilter_h
#define OVR_SensorFilter_h

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_Deque.h"
#include "Kernel/OVR_Alg.h"

namespace OVR {

// A base class for filters that maintains a buffer of sensor data taken over time and implements
// various simple filters, most of which are linear functions of the data history.
// Maintains the running sum of its elements for better performance on large capacity values
template <typename T>
class SensorFilterBase : public CircularBuffer<T>
{
protected:
    T RunningTotal;               // Cached sum of the elements

public:
    SensorFilterBase(int capacity = CircularBuffer<T>::DefaultCapacity)
        : CircularBuffer<T>(capacity), RunningTotal() 
    {
        this->Clear();
    };

    // The following methods are augmented to update the cached running sum value
    void PushBack(const T &e)
    {
        CircularBuffer<T>::PushBack(e);
        RunningTotal += e;
        if (this->End == 0)
        {
            // update the cached total to avoid error accumulation
            RunningTotal = T();
            for (int i = 0; i < this->ElemCount; i++)
                RunningTotal += this->Data[i];
        } 
    }

    void PushFront(const T &e)
    {
        CircularBuffer<T>::PushFront(e);
        RunningTotal += e;
        if (this->Beginning == 0)
        {
            // update the cached total to avoid error accumulation
            RunningTotal = T();
            for (int i = 0; i < this->ElemCount; i++)
                RunningTotal += this->Data[i];
        }
    }

    T PopBack() 
    { 
        T e = CircularBuffer<T>::PopBack();
        RunningTotal -= e;
        return e;
    }

    T PopFront() 
    { 
        T e = CircularBuffer<T>::PopFront();
        RunningTotal -= e;
        return e;
    }

    void Clear()
    {
        CircularBuffer<T>::Clear();
        RunningTotal = T();
    }

    // Simple statistics
    T Total() const 
    { 
        return RunningTotal; 
    }

    T Mean() const
    {
        return this->IsEmpty() ? T() : (Total() / (float) this->ElemCount);
    }

	T MeanN(int n) const
	{
        OVR_ASSERT(n > 0);
        OVR_ASSERT(this->Capacity >= n);
		T total = T();
        for (int i = 0; i < n; i++) 
        {
			total += this->PeekBack(i);
		}
		return total / n;
	}

    // A popular family of smoothing filters and smoothed derivatives

    T SavitzkyGolaySmooth4() 
    {
        OVR_ASSERT(this->Capacity >= 4);
        return this->PeekBack(0)*0.7f +
               this->PeekBack(1)*0.4f +
               this->PeekBack(2)*0.1f -
               this->PeekBack(3)*0.2f;
    }

    T SavitzkyGolaySmooth8() const
    {
        OVR_ASSERT(this->Capacity >= 8);
        return this->PeekBack(0)*0.41667f +
               this->PeekBack(1)*0.33333f +
               this->PeekBack(2)*0.25f +
               this->PeekBack(3)*0.16667f +
               this->PeekBack(4)*0.08333f -
               this->PeekBack(6)*0.08333f -
               this->PeekBack(7)*0.16667f;
    }

    T SavitzkyGolayDerivative4() const
    {
        OVR_ASSERT(this->Capacity >= 4);
        return this->PeekBack(0)*0.3f +
               this->PeekBack(1)*0.1f -
               this->PeekBack(2)*0.1f -
               this->PeekBack(3)*0.3f;
    }

    T SavitzkyGolayDerivative5() const
    {
            OVR_ASSERT(this->Capacity >= 5);
            return this->PeekBack(0)*0.2f +
                   this->PeekBack(1)*0.1f -
                   this->PeekBack(3)*0.1f -
                   this->PeekBack(4)*0.2f;
   }

    T SavitzkyGolayDerivative12() const
    {
        OVR_ASSERT(this->Capacity >= 12);
        return this->PeekBack(0)*0.03846f +
               this->PeekBack(1)*0.03147f +
               this->PeekBack(2)*0.02448f +
               this->PeekBack(3)*0.01748f +
               this->PeekBack(4)*0.01049f +
               this->PeekBack(5)*0.0035f -
               this->PeekBack(6)*0.0035f -
               this->PeekBack(7)*0.01049f -
               this->PeekBack(8)*0.01748f -
               this->PeekBack(9)*0.02448f -
               this->PeekBack(10)*0.03147f -
               this->PeekBack(11)*0.03846f;
    } 

    T SavitzkyGolayDerivativeN(int n) const
    {    
        OVR_ASSERT(this->capacity >= n);
        int m = (n-1)/2;
        T result = T();
        for (int k = 1; k <= m; k++) 
        {
            int ind1 = m - k;
            int ind2 = n - m + k - 1;
            result += (this->PeekBack(ind1) - this->PeekBack(ind2)) * (float) k;
        }
        float coef = 3.0f/(m*(m+1.0f)*(2.0f*m+1.0f));
        result = result*coef;
        return result;
    }

    T Median() const
    {
        T* copy = (T*) OVR_ALLOC(this->ElemCount * sizeof(T));
        T result = Alg::Median(ArrayAdaptor(copy));
        OVR_FREE(copy);
        return result;
    }
};

// This class maintains a buffer of sensor data taken over time and implements
// various simple filters, most of which are linear functions of the data history.
template <typename T>
class SensorFilter : public SensorFilterBase<Vector3<T> >
{
public:
	SensorFilter(int capacity = SensorFilterBase<Vector3<T> >::DefaultCapacity) : SensorFilterBase<Vector3<T> >(capacity) { };

    // Simple statistics
    Vector3<T> Median() const;
    Vector3<T> Variance() const; // The diagonal of covariance matrix
    Matrix3<T> Covariance() const;
    Vector3<T> PearsonCoefficient() const;
};

typedef SensorFilter<float> SensorFilterf;
typedef SensorFilter<double> SensorFilterd;

// This filter operates on the values that are measured in the body frame and rotate with the device
class SensorFilterBodyFrame : public SensorFilterBase<Vector3d>
{
private:
    // low pass filter gain
    double gain;
    // sum of squared norms of the values
    double runningTotalLengthSq;
    // cumulative rotation quaternion
    Quatd Q;
    // current low pass filter output
    Vector3d output;

    // make private so it isn't used by accident
    // in addition to the normal SensorFilterBase::PushBack, keeps track of running sum of LengthSq
    // for the purpose of variance computations
    void PushBack(const Vector3d &e)
    {
        runningTotalLengthSq += this->IsFull() ? (e.LengthSq() - this->PeekFront().LengthSq()) : e.LengthSq();
        SensorFilterBase<Vector3d>::PushBack(e);
        if (this->End == 0)
        {
            // update the cached total to avoid error accumulation
            runningTotalLengthSq = 0;
            for (int i = 0; i < this->ElemCount; i++)
                runningTotalLengthSq += this->Data[i].LengthSq();
        } 
    }

public:
	SensorFilterBodyFrame(int capacity = SensorFilterBase<Vector3d>::DefaultCapacity) 
        : SensorFilterBase<Vector3d>(capacity), gain(2.5), 
        runningTotalLengthSq(0), Q(), output()  { };

    // return the scalar variance of the filter values (rotated to be in the same frame)
    double Variance() const
    {
        return this->IsEmpty() ? 0 : (runningTotalLengthSq / this->ElemCount - this->Mean().LengthSq());
    }
    
    // return the scalar standard deviation of the filter values (rotated to be in the same frame)
    double StdDev() const
    {
        return sqrt(Variance());
    }

    // confidence value based on the stddev of the data (between 0.0 and 1.0, more is better)
    double Confidence() const
    {
        return Alg::Clamp(0.48 - 0.1 * log(StdDev()), 0.0, 1.0) * this->ElemCount / this->Capacity;
    }

    // add a new element to the filter
    // takes rotation increment since the last update 
    // in order to rotate the previous value to the current body frame
    void Update(Vector3d value, double deltaT, Quatd deltaQ = Quatd())
    {
        if (this->IsEmpty())
        {
            output = value;
        }
        else 
        {
            // rotate by deltaQ
            output = deltaQ.Inverted().Rotate(output);
            // apply low-pass filter
            output += (value - output) * gain * deltaT;
        }
        
        // put the value into the fixed frame for the stddev computation
        Q = Q * deltaQ;
        PushBack(Q.Rotate(output));
    }

    // returns the filter average in the current body frame
    Vector3d GetFilteredValue() const
    {
        return Q.Inverted().Rotate(this->Mean());
    }
};

} //namespace OVR

#endif // OVR_SensorFilter_h
