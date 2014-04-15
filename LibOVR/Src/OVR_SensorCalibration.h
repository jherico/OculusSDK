/************************************************************************************

Filename    :   OVR_SensorCalibration.h
Content     :   Calibration data implementation for the IMU messages 
Created     :   January 28, 2014
Authors     :   Max Katsev

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

#ifndef OVR_SensorCalibration_h
#define OVR_SensorCalibration_h

#include "OVR_Device.h"
#include "OVR_SensorFilter.h"

namespace OVR {

class OffsetInterpolator
{
public:
    void Initialize(Array<Array<TemperatureReport> > const& temperatureReports, int coord);
    double GetOffset(double targetTemperature, double autoTemperature, double autoValue);

    Array<double> Temperatures;
    Array<double> Values;
};

class SensorCalibration : public NewOverrideBase
{
public:
    SensorCalibration(SensorDevice* pSensor);

    // Load data from the HW and perform the necessary preprocessing
    void Initialize();
    // Apply the calibration
    void Apply(MessageBodyFrame& msg);

protected:
    void StoreAutoOffset();
    void AutocalibrateGyro(MessageBodyFrame const& msg);

    SensorDevice* pSensor;

    // Factory calibration data
    bool MagCalibrated;
    Matrix4f AccelMatrix, GyroMatrix, MagMatrix;
    Vector3f AccelOffset;
    
    // Temperature based data
    Array<Array<TemperatureReport> > temperatureReports;
    OffsetInterpolator Interpolators[3];

    // Autocalibration data
    SensorFilterf GyroFilter;
    Vector3f GyroAutoOffset;
    float GyroAutoTemperature;
};

} // namespace OVR
#endif //OVR_SensorCalibration_h
