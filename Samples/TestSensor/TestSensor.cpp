#include "OVR.h"

#ifdef WIN32
#define sleep(x) Sleep(1000 * x)
#else
#include <unistd.h>
#endif


using namespace OVR;

class TrackerHandler : public MessageHandler {
public:
    int count;
    TrackerHandler() : count(0) {};
    virtual void OnMessage(const Message& msg) {
        ++count;
        const MessageBodyFrame & trackerMessage = *(MessageBodyFrame*) &msg;
        const Vector3f & accel = trackerMessage.Acceleration;
        OVR_DEBUG_LOG(("X %0.3f Y %0.3f Z %0.3f Length %0.3f", accel.x, accel.y, accel.z, accel.Length()));
    }

    virtual bool SupportsMessageType(MessageType type) const {
        return Message_BodyFrame == type;
    }
};


int main(int argc, char ** argv) {
    System::Init();
    Log & log = *Log::GetDefaultLog();
    log.SetLoggingMask(LogMask_All);


    // *** Initialization - Create the first available HMD Device
    LogText("Attempting to instantiate the device manager\n");
    Ptr<DeviceManager> pManager = *DeviceManager::Create();
    if (!pManager) {
        LogError("Could not instantiate device manager.\n");
        return -1;
    }


    LogText("Attempting to instantiate the sensor device\n");
    Ptr<SensorDevice> pSensor = *pManager->EnumerateDevices<SensorDevice>().CreateDevice();
    if (!pSensor) {
        LogError("Could not instantiate sensor device.");
        return -1;
    }

    LogText("Attaching message handler to the device the sensor device\n");
    TrackerHandler handler;
    pSensor->SetMessageHandler(&handler);
    LogText("Waiting for messages for 1 second\n");

    sleep(1);
    LogText("Shutting down sensor device\n");
    pSensor.Clear();
    LogText("Received %d messages\n", handler.count);
    LogText("Shutting down DeviceManager\n");
    pManager.Clear();
    LogText("Shutting down OVR SDK\n");
    OVR::System::Destroy();
    LogText("Done\n");
    return 0;
}
