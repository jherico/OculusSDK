package org.saintandreas.oculus.fusion;

import org.saintandreas.math.Matrix4f;
import org.saintandreas.math.Quaternion;
import org.saintandreas.math.Vector3f;
import org.saintandreas.oculus.tracker.TrackerMessage;
import org.saintandreas.oculus.tracker.TrackerSample;
import org.saintandreas.oculus.tracker.TrackerVector;

import com.google.common.base.Predicate;

// -------------------------------------------------------------------------------------
// ***** SensorFusion

// SensorFusion class accumulates Sensor notification messages to keep track of
// orientation, which involves integrating the gyro and doing correction with
// gravity.
//
// Magnetometer based yaw drift correction is also supported; it is usually
// enabled automatically based on loaded magnetometer configuration.
//
// Orientation is reported as a quaternion, from which users can obtain either
// the rotation matrix or Euler angles.
//
// The class can operate in two ways:
// - By user manually passing MessageBodyFrame messages to the OnMessage()
// function.
// - By attaching SensorFusion to a SensorDevice, in which case it will
// automatically handle notifications from that device.

@SuppressWarnings("unused")
public class SensorFusion {
  private boolean yawCorrectionEnabled = false;
  private boolean motionTrackingEnabled = true;
  private boolean gravityEnabled = true;
  private boolean predictionEnabled = true;

  private Quaternion Q = new Quaternion();
  private Quaternion QUncorrected = new Quaternion();

  private Vector3f RawAccel = new Vector3f();
  private Vector3f AngV = new Vector3f();
  private float AngVL = 0;
  private Vector3f CalMag = new Vector3f();
  private Vector3f RawMag = new Vector3f();

  private int Stage = 0;
  private float RunningTime = 0;
  private float DeltaT = 0.001f;
  private float Gain = 0.05f;
  private float YawMult = 1.0f;
  private float PredictionDT = 0.03f;

  private final SensorFilter FRawMag = new SensorFilter(10);
  private final SensorFilter FAccW = new SensorFilter(20);
  private final SensorFilter FAngV = new SensorFilter(20);

  private int TiltCondCount = 0;
  private float TiltErrorAngle = 0;
  private Vector3f TiltErrorAxis = Vector3f.UNIT_Y;

  // Obtain the current accumulated orientation. Many apps will want to use
  // GetPredictedOrientation instead to reduce latency.
  public synchronized Quaternion getOrientation() {
    return Q;
  }

  public Quaternion getPredictedOrientation() {
    return getPredictedOrientation(PredictionDT);
  }

  // Obtain the last absolute acceleration reading, in m/s^2.
  public synchronized Vector3f getAcceleration() {
    return RawAccel;
  }

  // Obtain the last angular velocity reading, in rad/s.
  public synchronized Vector3f getAngularVelocity() {
    return AngV;
  }

  // Obtain the last raw magnetometer reading, in Gauss
  public synchronized Vector3f getMagnetometer() {
    return RawMag;
  }

  // Obtain the calibrated magnetometer reading (direction and field strength)
  synchronized Vector3f getCalibratedMagnetometer() {
    return CalMag;
  }

  // Resets the current orientation.
  public void setMotionTrackingEnabled(boolean enable) {
    motionTrackingEnabled = enable;
  }

  public boolean isMotionTrackingEnabled() {
    return motionTrackingEnabled;
  }

  // Multiplier for yaw rotation (turning); setting this higher than 1 (the
  // default) can allow the game
  // to be played without auxillary rotation controls, possibly making it more
  // immersive.
  // Whether this is more or less likely to cause motion sickness is unknown.
  public float GetYawMultiplier() {
    return YawMult;
  }

  public void SetYawMultiplier(float y) {
    YawMult = y;
  }

  // *** Prediction Control

  // Prediction delta specifes how much prediction should be applied in
  // seconds; it should in general be under the average rendering latency.
  // Call GetPredictedOrientation() to get predicted orientation.
  public float GetPredictionDelta() {
    return PredictionDT;
  }

  public void SetPrediction(float dt) {
    PredictionDT = dt;
    predictionEnabled = true;
  }

  public void SetPredictionEnabled(boolean enable) {
    predictionEnabled = enable;
  }

  public boolean IsPredictionEnabled() {
    return predictionEnabled;
  }

  // *** Accelerometer/Gravity Correction Control

  // Enables/disables gravity correction (on by default).
  public void SetGravityEnabled(boolean enable) {
    gravityEnabled = enable;
  }

  public boolean IsGravityEnabled() {
    return gravityEnabled;
  }

  // Gain used to correct gyro with accel. Default value is appropriate for
  // typical use.
  public float GetAccelGain() {
    return Gain;
  }

  public void SetAccelGain(float ag) {
    Gain = ag;
  }

  // *** Magnetometer and Yaw Drift Correction Control

  // Methods to load and save a mag calibration. Calibrations can optionally
  // be specified by name to differentiate multiple calibrations under
  // different conditions
  // If LoadMagCalibration succeeds, it will override YawCorrectionEnabled
  // based on
  // saved calibration setting.
  public boolean SaveMagCalibration(String file) {
    return false;
  }

  public boolean LoadMagCalibration(String file) {
    return false;
  }

  // Enables/disables magnetometer based yaw drift correction. Must also have
  // mag calibration data for this correction to work. void
  public void SetYawCorrectionEnabled(boolean enable) {
    yawCorrectionEnabled = enable;
  }

  // Determines if yaw correction is enabled.
  public boolean IsYawCorrectionEnabled() {
    return yawCorrectionEnabled;
  }

  // -------------------------------------------------------------------------------------
  // ***** Sensor Fusion

  // Resets the current orientation
  public synchronized void Reset() {
    QUncorrected = Q = Quaternion.IDENTITY;
    Stage = 0;
    RunningTime = 0;
    // MagNumReferences = 0;
    // MagHasNearbyReference = false;
  }

  // Sensor reports data in the following coordinate system:
  // Accelerometer: 10^-4 m/s^2; X forward, Y right, Z Down.
  // Gyro: 10^-4 rad/s; X positive roll right, Y positive pitch up; Z positive
  // yaw right.
  void AccelFromBodyFrameUpdate(TrackerVector in) {
    RawAccel = new Vector3f(in.x, in.y, in.z).mult(0.0001f);
  }

  void MagFromBodyFrameUpdate(TrackerVector in) {
    RawMag = new Vector3f(in.x, in.y, in.z).mult(0.0001f);
  }

  void EulerFromBodyFrameUpdate(TrackerVector in) {
    AngV = new Vector3f(in.x, in.y, in.z).mult(0.0001f);
    AngVL = AngV.length();
  }

  class FusionHandler implements Predicate<TrackerMessage> {
    private long LastTimestamp = -1;
    private int LastSampleCount = -1;

    // private final Vector3f LastMag = new Vector3f();
    // private final Vector3f LastGyro = new Vector3f();
    // private final Vector3f LastAccel = new Vector3f();

    @Override
    public boolean apply(TrackerMessage t) {
      synchronized (SensorFusion.this) {
        if (LastTimestamp != -1) {
          long timestampDelta = t.timestamp - LastTimestamp;
          if (t.timestamp < LastTimestamp) {
            timestampDelta += 0x10000;
          }

          // If we missed a small number of samples, replicate the
          // last sample.
          if ((timestampDelta > LastSampleCount) && (timestampDelta <= 254)) {
            DeltaT = (timestampDelta - t.sampleCount) * 0.001f;
            processSensorMessage();
            DeltaT = 0.001f;
          }
        }

        LastSampleCount = t.sampleCount;
        LastTimestamp = t.timestamp;
        MagFromBodyFrameUpdate(t.mag);
        // We've exceeded the max number of reportable samples, so
        for (int i = 0; i < Math.min(t.sampleCount, 3); ++i) {
          TrackerSample ts = t.samples.get(0);
          EulerFromBodyFrameUpdate(ts.gyro);
          AccelFromBodyFrameUpdate(ts.accel);
          processSensorMessage();
        }
      }
      return true;
    }
  }

  public FusionHandler createHandler() {
    return new FusionHandler();
  }

  private static final float GRAVITY_EPSILON = 0.4f;
  // Relatively slow rotation
  private static final float GYRO_EPSILON = 0.1f;
  // Required time steps of stability
  private static final int TILT_PERIOD = 50;
  private static final float MAX_TILT_ERROR = 0.05f;
  private static final float MIN_TILT_ERROR = 0.01f;

  public void processSensorMessage() {
    if (!isMotionTrackingEnabled())
      return;

    // Apply the calibration parameters to raw mag
    // if (HasMagCalibration())
    // {
    // mag.x += MagCalibrationMatrix.M[0][3];
    // mag.y += MagCalibrationMatrix.M[1][3];
    // mag.z += MagCalibrationMatrix.M[2][3];
    // }
    // // Provide external access to calibrated mag values
    // // (if the mag is not calibrated, then the raw value is returned)
    // CalMag = mag;

    // Keep track of time
    Stage++;
    RunningTime += DeltaT;

    // Insert current sensor data into filter history
    FRawMag.AddElement(RawMag);
    // Acceleration in the world frame (Q is current HMD orientation)
    FAccW.AddElement(Q.mult(RawAccel));
    FAngV.AddElement(AngV);

    // Update orientation Q based on gyro outputs. This technique is
    // based on direct properties of the angular velocity vector:
    // Its direction is the current rotation axis, and its magnitude
    // is the rotation rate (rad/sec) about that axis. Our sensor
    // sampling rate is so fast that we need not worry about integral
    // approximation error (not yet, anyway).
    if (AngVL > 0.0f) {
      Q = Q.mult(Quaternion.fromAngleAxis(AngVL * DeltaT, AngV.divide(AngVL)));
    }

    // The quaternion magnitude may slowly drift due to numerical error,
    // so it is periodically normalized.
    if (Stage % 5000 == 0) {
      Q = Q.normalize();
    }

    // Maintain the uncorrected orientation for later use by predictive
    // filtering
    QUncorrected = Q;

    // Perform tilt correction using the accelerometer data. This enables
    // drift errors in pitch and roll to be corrected. Note that yaw cannot
    // be corrected because the rotation axis is parallel to the gravity
    // vector.
    if (gravityEnabled) {
      // Correcting for tilt error by using accelerometer data
      tiltCorrect();
    } // Tilt correction

    // if (yawCorrectionEnabled) {
    // yawCorrect();
    // }

    // Warning: If YawMult != 1, then AngV is not true angular velocity
    AngV = new Vector3f(AngV.x, AngV.y * YawMult, AngV.z);
  }

  private static final float MIN_PDT = 0.001f;
  private static final float SLOPE_PDT = 0.1f;

  private void tiltCorrect() {
    // This condition estimates whether the only measured acceleration
    // is due to gravity
    // (the Rift is not linearly accelerating). It is often wrong, but
    // tends to average
    // out well over time.
    float accLength = RawAccel.length();
    if ((Math.abs(accLength - 9.81f) < GRAVITY_EPSILON)
        && (AngVL < GYRO_EPSILON))
      TiltCondCount++;
    else
      TiltCondCount = 0;

    // After stable measurements have been taken over a sufficiently
    // long period, estimate the amount of tilt error and calculate the
    // tilt axis for later correction.
    if (TiltCondCount >= TILT_PERIOD) {
      // Update TiltErrorEstimate
      TiltCondCount = 0;
      // Use an average value to reduce noise (could alternatively use
      // an LPF)
      Vector3f accWMean = FAccW.Mean();
      // This is the amount of rotation
      float tiltAngle = Vector3f.UNIT_Y.angleBetween(accWMean.normalize());
      // Record values if the tilt error is intolerable
      if (tiltAngle > MAX_TILT_ERROR) {
        // Project the acceleration vector into the XZ plane
        // The unit normal of xzAcc will be the rotation axis for
        // tilt correction
        TiltErrorAngle = tiltAngle;
        TiltErrorAxis = new Vector3f(accWMean.z, 0.0f, -accWMean.x)
            .normalize();
      }
    }

    // This part performs the actual tilt correction as needed
    if (TiltErrorAngle > MIN_TILT_ERROR) {
      if ((TiltErrorAngle > 0.4f) && (RunningTime < 8.0f)) {
        // Tilt completely to correct orientation
        Q = Q.mult(Quaternion.fromAngleAxis(-TiltErrorAngle, TiltErrorAxis));
        TiltErrorAngle = 0.0f;
      } else {
        // This uses aggressive correction steps while your head is
        // moving fast
        float deltaTiltAngle = -Gain * TiltErrorAngle * 0.005f
            * (5.0f * AngVL + 1.0f);
        // Incrementally "un-tilt" by a small step size
        Q = Q.mult(Quaternion.fromAngleAxis(deltaTiltAngle, TiltErrorAxis));
        TiltErrorAngle += deltaTiltAngle;
      }
    }
  }

  // A predictive filter based on extrapolating the smoothed, current angular
  // velocity
  public synchronized Quaternion getPredictedOrientation(float pdt) {
    Quaternion qP = QUncorrected;
    if (predictionEnabled) {
      // This method assumes a constant angular velocity
      // Vector3f angVelF = FAngV.SavitzkyGolaySmooth8();
      // Force back to raw measurement
      Vector3f angVelF = AngV;
      float angVelFL = angVelF.length();

      // Dynamic prediction interval: Based on angular velocity to reduce
      // vibration
      float newpdt = pdt;
      float tpdt = MIN_PDT + SLOPE_PDT * angVelFL;
      if (tpdt < pdt) {
        newpdt = tpdt;
      }

      if (angVelFL > 0.001f) {
        qP = qP.mult(Quaternion.fromAngleAxis(angVelFL * newpdt,
            angVelF.divide(angVelFL)));
      }
    }
    return qP;
  }

  private static final float maxAngVelLength = 3.0f;
  private static final int magWindow = 5;
  private static final float yawErrorMax = 0.1f;
  private static final float yawErrorMin = 0.01f;
  private static final int yawErrorCountLimit = 50;
  private static final float yawRotationStep = 0.00002f;
  private static final int MagMaxReferences = 80;

  public class MagCalibration {
    Matrix4f MagCalibrationMatrix = new Matrix4f();
    long MagCalibrationTime = -1;;
    boolean MagCalibrated = false;
    int MagCondCount = 0;
    float MagRefDistance = 0.5f;
    Quaternion MagRefQ = new Quaternion();
    Vector3f MagRefM = Vector3f.ZERO;
    float MagRefYaw = 0;
    boolean MagHasNearbyReference = false;
    final Quaternion[] MagRefTableQ = new Quaternion[MagMaxReferences];
    final Vector3f[] MagRefTableM = new Vector3f[MagMaxReferences];
    final float[] MagRefTableYaw = new float[MagMaxReferences];
    int MagNumReferences = 0;
    float YawErrorAngle = 0;
    int YawErrorCount = 0;
    boolean YawCorrectionInProgress = false;
    boolean YawCorrectionActivated = false;

    Vector3f getCalibratedMagValue(Vector3f rawMag) {
      Vector3f mag = rawMag;
      // mag.x += MagCalibrationMatrix.M[0][3];
      // mag.y += MagCalibrationMatrix.M[1][3];
      // mag.z += MagCalibrationMatrix.M[2][3];
      return mag;
    }

    void setMagReference(Quaternion q, Vector3f rawMag) {
      if (MagNumReferences < MagMaxReferences) {
        MagRefTableQ[MagNumReferences] = q;
        MagRefTableM[MagNumReferences] = rawMag; // FRawMag.Mean();
        MagRefQ = q;
        MagRefM = rawMag;
        float yaw = q.toAngles(null)[0];
        MagRefTableYaw[MagNumReferences] = yaw;
        MagRefYaw = yaw;
        MagNumReferences++;

        MagHasNearbyReference = true;
      }
    }

    // Yaw correction is currently working (forcing a corrective yaw *
    // rotation)
    boolean IsYawCorrectionInProgress() {
      return YawCorrectionInProgress;
    }

    // Store the calibration matrix for the magnetometer
    void SetMagCalibration(Matrix4f m) {
      MagCalibrationMatrix = m;
      MagCalibrationTime = System.currentTimeMillis();
      MagCalibrated = true;
    }

    // Retrieves the magnetometer calibration matrix
    Matrix4f GetMagCalibration() {
      return MagCalibrationMatrix;
    }

    // Retrieve the time of the calibration
    long GetMagCalibrationTime() {
      return MagCalibrationTime;
    }

    // True only if the mag has calibration values stored
    boolean HasMagCalibration() {
      return MagCalibrated;
    }

    // Force the mag into the uncalibrated state
    void ClearMagCalibration() {
      MagCalibrated = false;
    }

    // These refer to reference points that associate mag readings with
    // orientations
    void ClearMagReferences() {
      MagNumReferences = 0;
    }

    void SetMagRefDistance(float d) {
      MagRefDistance = d;
    }

    float GetMagRefYaw() {
      return MagRefYaw;
    }

    float GetYawErrorAngle() {
      return YawErrorAngle;
    }

    void yawCorrect() {
      // Yaw drift correction based on magnetometer data. This corrects
      // the
      // part of the drift that the accelerometer cannot handle. This will
      // only work if the magnetometer has been enabled, calibrated, and a
      // reference point has been set.

      if (AngVL < maxAngVelLength) {
        MagCondCount++;
      } else {
        MagCondCount = 0;
      }

      // Find, create, and utilize reference points for the magnetometer
      // Need
      // to be careful not to set reference points while there is
      // significant
      // tilt error
      if ((MagCalibrated) && (RunningTime > 10.0f) && (TiltErrorAngle < 0.2f)) {
        // if (MagNumReferences == 0) {
        // setMagReference(Q, RawMag); // Use the current direction
        // } else if (Q.Distance(MagRefQ) > MagRefDistance) {
        // MagHasNearbyReference = false;
        // float bestDist = 100000.0f;
        // int bestNdx = 0;
        // float dist;
        // for (int i = 0; i < MagNumReferences; i++) {
        // dist = Q.Distance(MagRefTableQ[i]);
        // if (dist < bestDist) {
        // bestNdx = i;
        // bestDist = dist;
        // }
        // }
        //
        // if (bestDist < MagRefDistance) {
        // MagHasNearbyReference = true;
        // MagRefQ = MagRefTableQ[bestNdx];
        // MagRefM = MagRefTableM[bestNdx];
        // MagRefYaw = MagRefTableYaw[bestNdx];
        // } else if (MagNumReferences < MagMaxReferences) {
        // setMagReference(Q, RawMag);
        // }
        // }
      }

      YawCorrectionInProgress = false;
      if (MagCalibrated && (RunningTime > 2.0f) && (MagCondCount >= magWindow)
          && MagHasNearbyReference) {
        // Use rotational invariance to bring reference mag value into
        // global frame
        Vector3f grefmag = MagRefQ.mult(getCalibratedMagValue(MagRefM));
        // Bring current (averaged) mag reading into global frame
        Vector3f gmag = Q.mult(getCalibratedMagValue(FRawMag.Mean()));
        // Calculate the reference yaw in the global frame
        float gryaw = (float) Math.atan2(grefmag.x, grefmag.z);
        // Calculate the current yaw in the global frame
        float gyaw = (float) Math.atan2(gmag.x, gmag.z);
        // The difference between reference and current yaws is the
        // perceived error
        // Anglef YawErrorAngle = gyaw - gryaw;
        //
        // // LogText("Yaw error estimate: %f\n",YawErrorAngle.Get());
        // // If the perceived error is large, keep count
        // if ((YawErrorAngle.Abs() > yawErrorMax) &&
        // (!YawCorrectionActivated)) {
        // YawErrorCount++;
        // }
        // // After enough iterations of high perceived error, start the
        // // correction process
        // if (YawErrorCount > yawErrorCountLimit) {
        // YawCorrectionActivated = true;
        // }
        // // If the perceived error becomes small, turn off the yaw
        // // correction
        // if ((YawErrorAngle.Abs() < yawErrorMin) &&
        // YawCorrectionActivated) {
        // YawCorrectionActivated = false;
        // YawErrorCount = 0;
        // }
        //
        // // Perform the actual yaw correction, due to previously
        // // detected,
        // // large yaw error
        // if (YawCorrectionActivated) {
        // YawCorrectionInProgress = true;
        // // Incrementally "unyaw" by a small step size
        // Q.multLocal(new Quaternion(-yawRotationStep *
        // YawErrorAngle.Sign(), Vector3f.UNIT_Y, true));
        // }
      }
    }

    // *** Message Handler Logic

    // // Notifies SensorFusion object about a new BodyFrame message from a
    // sensor.
    // // Should be called by user if not attaching to a sensor.
    // // Default to current HMD orientation
    // void setMagReference() {
    // setMagReference(Q, RawMag);
    // }

    // // Writes the current calibration for a particular device to a device
    // profile file
    // // sensor - the sensor that was calibrated
    // // cal_name - an optional name for the calibration or default if
    // cal_name
    // == NULL
    // bool SensorFusion::SaveMagCalibration(const char* calibrationName)
    // const
    // {
    // if (pSensor == NULL || !HasMagCalibration())
    // return false;
    //
    // // A named calibration may be specified for calibration in different
    // // environments, otherwise the default calibration is used
    // if (calibrationName == NULL)
    // calibrationName = "default";
    //
    // SensorInfo sinfo;
    // pSensor->GetDeviceInfo(&sinfo);
    //
    // // Generate a mag calibration event
    // JSON* calibration = JSON::CreateObject();
    // // (hardcoded for now) the measurement and representation method
    // calibration->AddStringItem("Version", "1.0");
    // calibration->AddStringItem("Name", "default");
    //
    // // time stamp the calibration
    // char time_str[64];
    //
    // #ifdef OVR_OS_WIN32
    // struct tm caltime;
    // localtime_s(&caltime, &MagCalibrationTime);
    // strftime(time_str, 64, "%Y-%m-%d %H:%M:%S", &caltime);
    // #else
    // struct tm* caltime;
    // caltime = localtime(&MagCalibrationTime);
    // strftime(time_str, 64, "%Y-%m-%d %H:%M:%S", caltime);
    // #endif
    //
    // calibration->AddStringItem("Time", time_str);
    //
    // // write the full calibration matrix
    // Matrix4f calmat = GetMagCalibration();
    // char matrix[128];
    // int pos = 0;
    // for (int r=0; r<4; r++)
    // {
    // for (int c=0; c<4; c++)
    // {
    // pos += (int)OVR_sprintf(matrix+pos, 128, "%g ", calmat.M[r][c]);
    // }
    // }
    // calibration->AddStringItem("Calibration", matrix);
    //
    //
    // String path = GetBaseOVRPath(true);
    // path += "/Devices.json";
    //
    // // Look for a prexisting device file to edit
    // Ptr<JSON> root = *JSON::Load(path);
    // if (root)
    // { // Quick sanity check of the file type and format before we parse
    // it
    // JSON* version = root->GetFirstItem();
    // if (version && version->Name == "Oculus Device Profile Version")
    // { // In the future I may need to check versioning to determine parse
    // method
    // }
    // else
    // {
    // root->Release();
    // root = NULL;
    // }
    // }
    //
    // JSON* device = NULL;
    // if (root)
    // {
    // device = root->GetFirstItem(); // skip the header
    // device = root->GetNextItem(device);
    // while (device)
    // { // Search for a previous calibration with the same name for this
    // device
    // // and remove it before adding the new one
    // if (device->Name == "Device")
    // {
    // JSON* item = device->GetItemByName("Serial");
    // if (item && item->Value == sinfo.SerialNumber)
    // { // found an entry for this device
    // item = device->GetNextItem(item);
    // while (item)
    // {
    // if (item->Name == "MagCalibration")
    // {
    // JSON* name = item->GetItemByName("Name");
    // if (name && name->Value == calibrationName)
    // { // found a calibration of the same name
    // item->RemoveNode();
    // item->Release();
    // break;
    // }
    // }
    // item = device->GetNextItem(item);
    // }
    //
    // // update the auto-mag flag
    // item = device->GetItemByName("EnableYawCorrection");
    // if (item)
    // item->dValue = (double)EnableYawCorrection;
    // else
    // device->AddBoolItem("EnableYawCorrection", EnableYawCorrection);
    //
    // break;
    // }
    // }
    //
    // device = root->GetNextItem(device);
    // }
    // }
    // else
    // { // Create a new device root
    // root = *JSON::CreateObject();
    // root->AddStringItem("Oculus Device Profile Version", "1.0");
    // }
    //
    // if (device == NULL)
    // {
    // device = JSON::CreateObject();
    // device->AddStringItem("Product", sinfo.ProductName);
    // device->AddNumberItem("ProductID", sinfo.ProductId);
    // device->AddStringItem("Serial", sinfo.SerialNumber);
    // device->AddBoolItem("EnableYawCorrection", EnableYawCorrection);
    //
    // root->AddItem("Device", device);
    // }
    //
    // // Create and the add the new calibration event to the device
    // device->AddItem("MagCalibration", calibration);
    //
    // return root->Save(path);
    // }
    //
    // // Loads a saved calibration for the specified device from the device
    // profile file
    // // sensor - the sensor that the calibration was saved for
    // // cal_name - an optional name for the calibration or the default if
    // cal_name == NULL
    // bool SensorFusion::LoadMagCalibration(const char* calibrationName)
    // {
    // if (pSensor == NULL)
    // return false;
    //
    // // A named calibration may be specified for calibration in different
    // // environments, otherwise the default calibration is used
    // if (calibrationName == NULL)
    // calibrationName = "default";
    //
    // SensorInfo sinfo;
    // pSensor->GetDeviceInfo(&sinfo);
    //
    // String path = GetBaseOVRPath(true);
    // path += "/Devices.json";
    //
    // // Load the device profiles
    // Ptr<JSON> root = *JSON::Load(path);
    // if (root == NULL)
    // return false;
    //
    // // Quick sanity check of the file type and format before we parse it
    // JSON* version = root->GetFirstItem();
    // if (version && version->Name == "Oculus Device Profile Version")
    // { // In the future I may need to check versioning to determine parse
    // method
    // }
    // else
    // {
    // return false;
    // }
    //
    // bool autoEnableCorrection = false;
    //
    // JSON* device = root->GetNextItem(version);
    // while (device)
    // { // Search for a previous calibration with the same name for this
    // device
    // // and remove it before adding the new one
    // if (device->Name == "Device")
    // {
    // JSON* item = device->GetItemByName("Serial");
    // if (item && item->Value == sinfo.SerialNumber)
    // { // found an entry for this device
    //
    // JSON* autoyaw = device->GetItemByName("EnableYawCorrection");
    // if (autoyaw)
    // autoEnableCorrection = (autoyaw->dValue != 0);
    //
    // item = device->GetNextItem(item);
    // while (item)
    // {
    // if (item->Name == "MagCalibration")
    // {
    // JSON* calibration = item;
    // JSON* name = calibration->GetItemByName("Name");
    // if (name && name->Value == calibrationName)
    // { // found a calibration with this name
    //
    // time_t now;
    // time(&now);
    //
    // // parse the calibration time
    // time_t calibration_time = now;
    // JSON* caltime = calibration->GetItemByName("Time");
    // if (caltime)
    // {
    // const char* caltime_str = caltime->Value.ToCStr();
    //
    // tm ct;
    // memset(&ct, 0, sizeof(tm));
    //
    // #ifdef OVR_OS_WIN32
    // struct tm nowtime;
    // localtime_s(&nowtime, &now);
    // ct.tm_isdst = nowtime.tm_isdst;
    // sscanf_s(caltime_str, "%d-%d-%d %d:%d:%d",
    // &ct.tm_year, &ct.tm_mon, &ct.tm_mday,
    // &ct.tm_hour, &ct.tm_min, &ct.tm_sec);
    // #else
    // struct tm* nowtime = localtime(&now);
    // ct.tm_isdst = nowtime->tm_isdst;
    // sscanf(caltime_str, "%d-%d-%d %d:%d:%d",
    // &ct.tm_year, &ct.tm_mon, &ct.tm_mday,
    // &ct.tm_hour, &ct.tm_min, &ct.tm_sec);
    // #endif
    // ct.tm_year -= 1900;
    // ct.tm_mon--;
    // calibration_time = mktime(&ct);
    // }
    //
    // // parse the calibration matrix
    // JSON* cal = calibration->GetItemByName("Calibration");
    // if (cal)
    // {
    // const char* data_str = cal->Value.ToCStr();
    // Matrix4f calmat;
    // for (int r=0; r<4; r++)
    // {
    // for (int c=0; c<4; c++)
    // {
    // calmat.M[r][c] = (float)atof(data_str);
    // while (data_str && *data_str != ' ')
    // data_str++;
    //
    // if (data_str)
    // data_str++;
    // }
    // }
    //
    // SetMagCalibration(calmat);
    // MagCalibrationTime = calibration_time;
    // EnableYawCorrection = autoEnableCorrection;
    //
    // return true;
    // }
    // }
    // }
    // item = device->GetNextItem(item);
    // }
    //
    // break;
    // }
    // }
    //
    // device = root->GetNextItem(device);
    // }
    //
    // return false;
    // }
    //
    //

  }
};
