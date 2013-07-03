/************************************************************************************

Filename    :   OVR_SensorFusion.cpp
Content     :   Methods that determine head orientation from sensor data over time
Created     :   October 9, 2012
Authors     :   Michael Antonov, Steve LaValle

Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.

Use of this software is subject to the terms of the Oculus license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

*************************************************************************************/

#include "OVR_SensorFusion.h"
#include "Kernel/OVR_Log.h"
#include "Kernel/OVR_System.h"

namespace OVR {

//-------------------------------------------------------------------------------------
// ***** Sensor Fusion

SensorFusion::SensorFusion(SensorDevice* sensor)
  : Handler(getThis()), pDelegate(0),
    Gain(0.05f), YawMult(1), EnableGravity(true), Stage(0), RunningTime(0), DeltaT(0.001f),
	EnablePrediction(true), PredictionDT(0.03f), PredictionTimeIncrement(0.001f),
    FRawMag(10), FAccW(20), FAngV(20),
    TiltCondCount(0), TiltErrorAngle(0), 
    TiltErrorAxis(0,1,0),
    MagCondCount(0), MagCalibrated(false), MagRefQ(0, 0, 0, 1), 
	MagRefM(0), MagRefYaw(0), YawErrorAngle(0), MagRefDistance(0.5f),
    YawErrorCount(0), YawCorrectionActivated(false), YawCorrectionInProgress(false), 
	EnableYawCorrection(false), MagNumReferences(0), MagHasNearbyReference(false)
{
   if (sensor)
       AttachToSensor(sensor);
   MagCalibrationMatrix.SetIdentity();
}

SensorFusion::~SensorFusion()
{
}


bool SensorFusion::AttachToSensor(SensorDevice* sensor)
{
    
    if (sensor != NULL)
    {
        MessageHandler* pCurrentHandler = sensor->GetMessageHandler();

        if (pCurrentHandler == &Handler)
        {
            Reset();
            return true;
        }

        if (pCurrentHandler != NULL)
        {
            OVR_DEBUG_LOG(
                ("SensorFusion::AttachToSensor failed - sensor %p already has handler", sensor));
            return false;
        }
    }

    if (Handler.IsHandlerInstalled())
    {
        Handler.RemoveHandlerFromDevices();
    }

    if (sensor != NULL)
    {
        sensor->SetMessageHandler(&Handler);
    }

    Reset();
    return true;
}


    // Resets the current orientation
void SensorFusion::Reset()
{
    Lock::Locker lockScope(Handler.GetHandlerLock());
    Q                     = Quatf();
    QUncorrected          = Quatf();
    Stage                 = 0;
	RunningTime           = 0;
	MagNumReferences      = 0;
	MagHasNearbyReference = false;
}


void SensorFusion::handleMessage(const MessageBodyFrame& msg)
{
    if (msg.Type != Message_BodyFrame)
        return;
  
    // Put the sensor readings into convenient local variables
    Vector3f angVel    = msg.RotationRate; 
    Vector3f rawAccel  = msg.Acceleration;
    Vector3f mag       = msg.MagneticField;

    // Set variables accessible through the class API
	DeltaT = msg.TimeDelta;
    AngV = msg.RotationRate;
    AngV.y *= YawMult;  // Warning: If YawMult != 1, then AngV is not true angular velocity
    A = rawAccel;

    // Allow external access to uncalibrated magnetometer values
    RawMag = mag;  

    // Apply the calibration parameters to raw mag
    if (HasMagCalibration())
    {
        mag.x += MagCalibrationMatrix.M[0][3];
        mag.y += MagCalibrationMatrix.M[1][3];
        mag.z += MagCalibrationMatrix.M[2][3];
    }

    // Provide external access to calibrated mag values
    // (if the mag is not calibrated, then the raw value is returned)
    CalMag = mag;

    float angVelLength = angVel.Length();
    float accLength    = rawAccel.Length();


    // Acceleration in the world frame (Q is current HMD orientation)
    Vector3f accWorld  = Q.Rotate(rawAccel);

    // Keep track of time
    Stage++;
    RunningTime += DeltaT;

    // Insert current sensor data into filter history
    FRawMag.AddElement(RawMag);
    FAccW.AddElement(accWorld);
    FAngV.AddElement(angVel);

    // Update orientation Q based on gyro outputs.  This technique is
    // based on direct properties of the angular velocity vector:
    // Its direction is the current rotation axis, and its magnitude
    // is the rotation rate (rad/sec) about that axis.  Our sensor
    // sampling rate is so fast that we need not worry about integral
    // approximation error (not yet, anyway).
    if (angVelLength > 0.0f)
    {
        Vector3f     rotAxis      = angVel / angVelLength;  
        float        halfRotAngle = angVelLength * DeltaT * 0.5f;
        float        sinHRA       = sin(halfRotAngle);
        Quatf        deltaQ(rotAxis.x*sinHRA, rotAxis.y*sinHRA, rotAxis.z*sinHRA, cos(halfRotAngle));

        Q =  Q * deltaQ;
    }
    
    // The quaternion magnitude may slowly drift due to numerical error,
    // so it is periodically normalized.
    if (Stage % 5000 == 0)
        Q.Normalize();
    
	// Maintain the uncorrected orientation for later use by predictive filtering
	QUncorrected = Q;

    // Perform tilt correction using the accelerometer data. This enables 
    // drift errors in pitch and roll to be corrected. Note that yaw cannot be corrected
    // because the rotation axis is parallel to the gravity vector.
    if (EnableGravity)
    {
        // Correcting for tilt error by using accelerometer data
        const float  gravityEpsilon = 0.4f;
        const float  angVelEpsilon  = 0.1f; // Relatively slow rotation
        const int    tiltPeriod     = 50;   // Required time steps of stability
        const float  maxTiltError   = 0.05f;
        const float  minTiltError   = 0.01f;

        // This condition estimates whether the only measured acceleration is due to gravity 
        // (the Rift is not linearly accelerating).  It is often wrong, but tends to average
        // out well over time.
        if ((fabs(accLength - 9.81f) < gravityEpsilon) &&
            (angVelLength < angVelEpsilon))
            TiltCondCount++;
        else
            TiltCondCount = 0;
    
        // After stable measurements have been taken over a sufficiently long period,
        // estimate the amount of tilt error and calculate the tilt axis for later correction.
        if (TiltCondCount >= tiltPeriod)
        {   // Update TiltErrorEstimate
            TiltCondCount = 0;
            // Use an average value to reduce noise (could alternatively use an LPF)
            Vector3f accWMean = FAccW.Mean();
            // Project the acceleration vector into the XZ plane
            Vector3f xzAcc = Vector3f(accWMean.x, 0.0f, accWMean.z);
            // The unit normal of xzAcc will be the rotation axis for tilt correction
            Vector3f tiltAxis = Vector3f(xzAcc.z, 0.0f, -xzAcc.x).Normalized();
            Vector3f yUp = Vector3f(0.0f, 1.0f, 0.0f);
            // This is the amount of rotation
            float    tiltAngle = yUp.Angle(accWMean);
            // Record values if the tilt error is intolerable
            if (tiltAngle > maxTiltError) 
            {
                TiltErrorAngle = tiltAngle;
                TiltErrorAxis = tiltAxis;
            }
        }

        // This part performs the actual tilt correction as needed
        if (TiltErrorAngle > minTiltError) 
        {
            if ((TiltErrorAngle > 0.4f)&&(RunningTime < 8.0f))
            {   // Tilt completely to correct orientation
                Q = Quatf(TiltErrorAxis, -TiltErrorAngle) * Q;
                TiltErrorAngle = 0.0f;
            }
            else 
            {
                //LogText("Performing tilt correction  -  Angle: %f   Axis: %f %f %f\n",
                //        TiltErrorAngle,TiltErrorAxis.x,TiltErrorAxis.y,TiltErrorAxis.z);
                //float deltaTiltAngle = -Gain*TiltErrorAngle*0.005f;
                // This uses aggressive correction steps while your head is moving fast
                float deltaTiltAngle = -Gain*TiltErrorAngle*0.005f*(5.0f*angVelLength+1.0f);
                // Incrementally "un-tilt" by a small step size
                Q = Quatf(TiltErrorAxis, deltaTiltAngle) * Q;
                TiltErrorAngle += deltaTiltAngle;
            }
        }
    }

    // Yaw drift correction based on magnetometer data.  This corrects the part of the drift
    // that the accelerometer cannot handle.
    // This will only work if the magnetometer has been enabled, calibrated, and a reference
    // point has been set.
    const float maxAngVelLength = 3.0f;
    const int   magWindow = 5;
    const float yawErrorMax = 0.1f;
    const float yawErrorMin = 0.01f;
    const int   yawErrorCountLimit = 50;
    const float yawRotationStep = 0.00002f;

    if (angVelLength < maxAngVelLength)
        MagCondCount++;
    else
        MagCondCount = 0;

	// Find, create, and utilize reference points for the magnetometer
	// Need to be careful not to set reference points while there is significant tilt error
    if ((EnableYawCorrection && MagCalibrated)&&(RunningTime > 10.0f)&&(TiltErrorAngle < 0.2f))
	{
	  if (MagNumReferences == 0)
      {
		  SetMagReference(); // Use the current direction
      }
	  else if (Q.Distance(MagRefQ) > MagRefDistance) 
      {
		  MagHasNearbyReference = false;
          float bestDist = 100000.0f;
          int bestNdx = 0;
          float dist;
          for (int i = 0; i < MagNumReferences; i++)
          {
              dist = Q.Distance(MagRefTableQ[i]);
              if (dist < bestDist)
              {
                  bestNdx = i;
                  bestDist = dist;
              }
          }

          if (bestDist < MagRefDistance)
          {
              MagHasNearbyReference = true;
              MagRefQ   = MagRefTableQ[bestNdx];
              MagRefM   = MagRefTableM[bestNdx];
              MagRefYaw = MagRefTableYaw[bestNdx];
              //LogText("Using reference %d\n",bestNdx);
          }
          else if (MagNumReferences < MagMaxReferences)
              SetMagReference();
	  }
	}

	YawCorrectionInProgress = false;
    if (EnableYawCorrection && MagCalibrated && (RunningTime > 2.0f) && (MagCondCount >= magWindow) &&
        MagHasNearbyReference)
    {
        // Use rotational invariance to bring reference mag value into global frame
        Vector3f grefmag = MagRefQ.Rotate(GetCalibratedMagValue(MagRefM));
        // Bring current (averaged) mag reading into global frame
        Vector3f gmag = Q.Rotate(GetCalibratedMagValue(FRawMag.Mean()));
        // Calculate the reference yaw in the global frame
        Anglef gryaw = Anglef(atan2(grefmag.x,grefmag.z));
        // Calculate the current yaw in the global frame
        Anglef gyaw = Anglef(atan2(gmag.x,gmag.z));
        // The difference between reference and current yaws is the perceived error
        Anglef YawErrorAngle = gyaw - gryaw;

        //LogText("Yaw error estimate: %f\n",YawErrorAngle.Get());
        // If the perceived error is large, keep count
        if ((YawErrorAngle.Abs() > yawErrorMax) && (!YawCorrectionActivated))
            YawErrorCount++;
        // After enough iterations of high perceived error, start the correction process
        if (YawErrorCount > yawErrorCountLimit)
            YawCorrectionActivated = true;
        // If the perceived error becomes small, turn off the yaw correction
        if ((YawErrorAngle.Abs() < yawErrorMin) && YawCorrectionActivated) 
        {
            YawCorrectionActivated = false;
            YawErrorCount = 0;
        }
        
        // Perform the actual yaw correction, due to previously detected, large yaw error
        if (YawCorrectionActivated) 
        {
			YawCorrectionInProgress = true;
            // Incrementally "unyaw" by a small step size
            Q = Quatf(Vector3f(0.0f,1.0f,0.0f), -yawRotationStep * YawErrorAngle.Sign()) * Q;
        }
    }
}

 
//  Simple predictive filters based on extrapolating the smoothed, current angular velocity
// or using smooth time derivative information.  The argument is the amount of time into
// the future to predict.
Quatf SensorFusion::GetPredictedOrientation(float pdt)
{		
	Lock::Locker lockScope(Handler.GetHandlerLock());
	Quatf        qP = QUncorrected;
	
    if (EnablePrediction)
    {
#if 1
		// This method assumes a constant angular velocity
	    Vector3f angVelF  = FAngV.SavitzkyGolaySmooth8();
        float    angVelFL = angVelF.Length();
            
        if (angVelFL > 0.001f)
        {
            Vector3f    rotAxisP      = angVelF / angVelFL;  
            float       halfRotAngleP = angVelFL * pdt * 0.5f;
            float       sinaHRAP      = sin(halfRotAngleP);
		    Quatf       deltaQP(rotAxisP.x*sinaHRAP, rotAxisP.y*sinaHRAP,
                                rotAxisP.z*sinaHRAP, cos(halfRotAngleP));
            qP = QUncorrected * deltaQP;
        }
#else
		// This method estimates angular acceleration, conservatively
		OVR_ASSERT(pdt >= 0);
        int       predictionStages = (int)(pdt / PredictionTimeIncrement + 0.5f);
        Quatd     qpd        = Quatd(Q.x,Q.y,Q.z,Q.w);
        Vector3f  aa         = FAngV.SavitzkyGolayDerivative12();
        Vector3d  aad        = Vector3d(aa.x,aa.y,aa.z);
        Vector3f  angVelF    = FAngV.SavitzkyGolaySmooth8();
        Vector3d  avkd       = Vector3d(angVelF.x,angVelF.y,angVelF.z);
		Vector3d  rotAxisd   = Vector3d(0,1,0);
        for (int i = 0; i < predictionStages; i++)
        {
            double   angVelLengthd = avkd.Length();
			if (angVelLengthd > 0)
                rotAxisd = avkd / angVelLengthd;
            double   halfRotAngled = angVelLengthd * PredictionTimeIncrement * 0.5;
            double   sinHRAd       = sin(halfRotAngled);
            Quatd    deltaQd       = Quatd(rotAxisd.x*sinHRAd, rotAxisd.y*sinHRAd, rotAxisd.z*sinHRAd,
                                           cos(halfRotAngled));
            qpd = qpd * deltaQd;
            // Update angular velocity by using the angular acceleration estimate
            avkd += aad;
        }
        qP = Quatf((float)qpd.x,(float)qpd.y,(float)qpd.z,(float)qpd.w);
#endif
	}
    return qP;
}    


Vector3f SensorFusion::GetCalibratedMagValue(const Vector3f& rawMag) const
{
    Vector3f mag = rawMag;
    OVR_ASSERT(HasMagCalibration());
    mag.x += MagCalibrationMatrix.M[0][3];
    mag.y += MagCalibrationMatrix.M[1][3];
    mag.z += MagCalibrationMatrix.M[2][3];
    return mag;
}


void SensorFusion::SetMagReference(const Quatf& q, const Vector3f& rawMag) 
{
    if (MagNumReferences < MagMaxReferences)
    {
        MagRefTableQ[MagNumReferences] = q;
        MagRefTableM[MagNumReferences] = rawMag; //FRawMag.Mean();

		//LogText("Inserting reference %d\n",MagNumReferences);
        
		MagRefQ   = q;
        MagRefM   = rawMag;

        float pitch, roll, yaw;
		Quatf q2 = q;
        q2.GetEulerAngles<Axis_X, Axis_Z, Axis_Y>(&pitch, &roll, &yaw);
        MagRefTableYaw[MagNumReferences] = yaw;
		MagRefYaw = yaw;

        MagNumReferences++;

        MagHasNearbyReference = true;
    }
}


SensorFusion::BodyFrameHandler::~BodyFrameHandler()
{
    RemoveHandlerFromDevices();
}

void SensorFusion::BodyFrameHandler::OnMessage(const Message& msg)
{
    if (msg.Type == Message_BodyFrame)
        pFusion->handleMessage(static_cast<const MessageBodyFrame&>(msg));
    if (pFusion->pDelegate)
        pFusion->pDelegate->OnMessage(msg);
}

bool SensorFusion::BodyFrameHandler::SupportsMessageType(MessageType type) const
{
    return (type == Message_BodyFrame);
}


} // namespace OVR

