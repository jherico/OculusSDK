#include "OVR_C.h"
#include "OVR.h"
#include <vector>
#include <memory>
#include <map>

using namespace std;
using namespace OVR;

void convertOvrVector(const Vector3f & in, OvrVector & out) {
    memcpy(&out.x, &in.x, sizeof(out.x) * 3);
}

void convertOvrQuat(const Quatf & in, OvrQuaternionf & out) {
    memcpy(&out.x, &in.x, sizeof(out.x) * 4);
}

typedef shared_ptr<SensorFusion> SensorFusionPtr;

class Rift : public MessageHandler {
public:
    OVR_SENSOR_CALLBACK callback;
    Ptr<HMDDevice> ovrHmd;
    Ptr<SensorDevice> ovrSensor;
    SensorFusionPtr sensorFusion;

    Rift(Ptr<HMDDevice> & ovrHmd, Ptr<SensorDevice> ovrSensor)
            : callback(nullptr), ovrHmd(ovrHmd), ovrSensor(ovrSensor) {
    }

    virtual ~Rift() {
        if (ovrSensor) {
            ovrSensor->SetMessageHandler(nullptr);
        }
        ovrHmd.Clear();
        ovrSensor.Clear();
    }

    OVR_SENSOR_CALLBACK setCallback(OVR_SENSOR_CALLBACK newCallback) {
        OVR_SENSOR_CALLBACK oldCallback = callback;
        callback = newCallback;
        if (callback) {
            ovrSensor->SetMessageHandler(this);
        } else {
            ovrSensor->SetMessageHandler(nullptr);
        }
        return oldCallback;
    }

    virtual void OnMessage(const Message& msg) {
        if (callback) {
            static OvrSensorMessage out;
            const MessageBodyFrame & in = *(MessageBodyFrame*) &msg;
            convertOvrVector(in.Acceleration, out.Accel);
            convertOvrVector(in.RotationRate, out.Gyro);
            convertOvrVector(in.MagneticField, out.Mag);
            out.TimeDelta = in.TimeDelta;
            callback(&out);
        }

    }

    virtual bool SupportsMessageType(MessageType type) const {
        return Message_BodyFrame == type;
    }

    void enableSensorFusion(bool gravityCorrection, bool magneticCorrection, bool prediction) {
        if (!ovrSensor) {
            return;
        }

        sensorFusion = SensorFusionPtr(new SensorFusion());
        sensorFusion->SetPredictionEnabled(prediction);
        sensorFusion->SetGravityEnabled(gravityCorrection);
        sensorFusion->SetYawCorrectionEnabled(magneticCorrection);
        sensorFusion->AttachToSensor(ovrSensor);
    }

    void getPredictionOrientation(float delta, OvrQuaternionf & out) {
        if (!sensorFusion) {
            return;
        }
        SensorFusion & f = *sensorFusion;
        Quatf q = f.GetPredictedOrientation(delta);
        convertOvrQuat(q, out);
    }

    void getPredictedEulerAngles(float delta, OvrVector3f & out) {
        if (!sensorFusion) {
            return;
        }
        SensorFusion & f = *sensorFusion;
        Quatf q = f.GetPredictedOrientation(delta);
        q.GetEulerAngles<Axis_X, Axis_Y, Axis_Z>(&out.x, &out.y, &out.z);
    }

    void getOrientation(OvrQuaternionf & out) {
        if (!sensorFusion) {
            return;
        }
        SensorFusion & f = *sensorFusion;
        Quatf q = f.GetOrientation();
        convertOvrQuat(q, out);
    }

    void getEulerAngles(OvrVector3f & out) {
        if (!sensorFusion) {
            return;
        }
        SensorFusion & f = *sensorFusion;
        Quatf q = f.GetOrientation();
        q.GetEulerAngles<Axis_X, Axis_Y, Axis_Z>(&out.x, &out.y, &out.z);
    }

    void resetSensorFusion() {
        if (!sensorFusion) {
            return;
        }
        SensorFusion & f = *sensorFusion;
        f.Reset();
    }
};

typedef shared_ptr<Rift> RiftPtr;

typedef vector<RiftPtr> RiftVector;

class RiftManager {
public:
    RiftVector rifts;
    Ptr<DeviceManager> ovrManager;

    RiftManager() {
        ovrManager = *DeviceManager::Create();
    }
    ~RiftManager() {
        rifts.clear();
        ovrManager.Clear();
    }

    OVR_HANDLE openRift(const char * serialNumber = nullptr) {
//        typedef DeviceEnumerator<HMDDevice> HMDEnum;
        Ptr<HMDDevice> ovrHmd = *ovrManager->EnumerateDevices<HMDDevice>().CreateDevice();
        Ptr<SensorDevice> ovrSensor = *ovrManager->EnumerateDevices<SensorDevice>().CreateDevice();

        if (!ovrSensor && !ovrHmd) {
            return 0;
        }
        RiftPtr rift = RiftPtr(new Rift(ovrHmd, ovrSensor));
        rifts.push_back(rift);
        return rifts.size();
    }

    RiftPtr getRift(OVR_HANDLE device) {
        if (rifts.size() < device) {
            return RiftPtr();
        }

        unsigned int index = device - 1;
        return rifts[index];
    }
};

typedef shared_ptr<RiftManager> RiftManagerPtr;

RiftManagerPtr MANAGER;
unsigned int OVR_ERROR = OVR_NO_ERROR;

extern "C" {

void OVR_STDCALL ovrInit() {
    System::Init();
    MANAGER = RiftManagerPtr(new RiftManager());
}

void OVR_STDCALL ovrDestroy() {
    MANAGER = nullptr;
    System::Destroy();
}

void ovrSetError(unsigned int error) {
    OVR_ERROR = error;
}

unsigned int OVR_STDCALL ovrGetError() {
    unsigned int result = OVR_ERROR;
    OVR_ERROR = OVR_NO_ERROR;
    return result;
}

OVR_HANDLE OVR_STDCALL ovrOpenFirstAvailableRift() {
    if (MANAGER->rifts.size() > 0) {
        return 1;
    }

    return MANAGER->openRift();
}

void OVR_STDCALL ovrCloseRift(OVR_HANDLE device) {
    RiftPtr rift = MANAGER->getRift(device);
    if (!rift) {
        ovrSetError(OVR_INVALID_PARAM);
        return;
    }

    rift = nullptr;
    return;
}

OVR_SENSOR_CALLBACK ovrRegisterSampleHandler(OVR_HANDLE device, OVR_SENSOR_CALLBACK newCallback) {
    RiftPtr rift = MANAGER->getRift(device);
    if (!rift) {
        ovrSetError(OVR_INVALID_PARAM);
        return nullptr;
    }
    return rift->setCallback(newCallback);
}

void OVR_STDCALL ovrEnableSensorFusion(OVR_HANDLE device, int enableGravityCorrection, int enableMagneticCorrection,
        int enablePrediction) {
    RiftPtr rift = MANAGER->getRift(device);
    if (!rift) {
        ovrSetError(OVR_INVALID_PARAM);
        return;
    }
    rift->enableSensorFusion(enableGravityCorrection ? true : false, enableMagneticCorrection ? true : false,
            enablePrediction ? true : false);
}

void OVR_STDCALL ovrGetPredictedOrientation(OVR_HANDLE device, float predictionDelta, OvrQuaternionf * out) {
    RiftPtr rift = MANAGER->getRift(device);
    if (!rift) {
        ovrSetError(OVR_INVALID_PARAM);
        return;
    }
    rift->getPredictionOrientation(predictionDelta, *out);
}

void OVR_STDCALL ovrGetPredictedEulerAngles(OVR_HANDLE device, float predictionDelta, OvrVector3f * out) {
    RiftPtr rift = MANAGER->getRift(device);
    if (!rift) {
        ovrSetError(OVR_INVALID_PARAM);
        return;
    }
    rift->getPredictedEulerAngles(predictionDelta, *out);
}

void OVR_STDCALL ovrGetOrientation(OVR_HANDLE device, OvrQuaternionf * out) {
    RiftPtr rift = MANAGER->getRift(device);
    if (!rift) {
        ovrSetError(OVR_INVALID_PARAM);
        return;
    }
    rift->getOrientation(*out);
}

void OVR_STDCALL ovrGetEulerAngles(OVR_HANDLE device, OvrVector3f * out) {
    RiftPtr rift = MANAGER->getRift(device);
    if (!rift) {
        ovrSetError(OVR_INVALID_PARAM);
        return;
    }
    rift->getEulerAngles(*out);
}

void OVR_STDCALL ovrResetSensorFusion(OVR_HANDLE device) {
    RiftPtr rift = MANAGER->getRift(device);
    if (!rift) {
        ovrSetError(OVR_INVALID_PARAM);
        return;
    }
    rift->resetSensorFusion();
}

}

