#include "OVR_C.h"
#ifdef WIN32
#define sleep(x) Sleep(1000 * x)
#define usleep(x) Sleep(x / 1000);
#else
#include <unistd.h>
#endif
#include <stdio.h>


void OVR_STDCALL HandleMessage(const OvrSensorMessage * message) {
    static int count = 0;
    if (0 == (count++ % 100)) {
        printf("X %0.3f Y %0.3f Z %0.3f\n",
                message->Accel.x, message->Accel.y, message->Accel.z);
    }
}

int main(int argc, char ** argv) {
    printf("Setting up the SDK\n");
    ovrInit();
    printf("Opening a rift device\n");
    OVR_HANDLE rift = ovrOpenFirstAvailableRift();
    if (rift) {
        printf("Found rift %d", rift);
        ovrRegisterSampleHandler(rift, HandleMessage);
        sleep(5);
        printf("Removing message handler\n");
        ovrRegisterSampleHandler(rift, 0);
        sleep(1);
        printf("Enabling sensor fusion\n");
        ovrEnableSensorFusion(rift, 1, 1, 1);
        int i = 0;
        OvrVector3f euler;
        for (i = 0; i < 1000; ++i) {
            ovrGetEulerAngles(rift, &euler);
            printf("Roll %0.3f Pitch %0.3f Yaw %0.3f\n",
                    euler.z, euler.x, euler.y);
            usleep(100 * 1000);
        }
        sleep(2);
        ovrCloseRift(rift);
    }
    ovrDestroy();
    return 0;
}
