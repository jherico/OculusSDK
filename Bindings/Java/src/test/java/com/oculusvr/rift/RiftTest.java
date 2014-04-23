package com.oculusvr.rift;

import java.io.IOException;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.oculusvr.api.OvrLibrary;
import com.oculusvr.api.OvrLibrary.ovrHmd;
import com.oculusvr.api.ovrSensorState_;

public class RiftTest {
  private static final Logger LOG = LoggerFactory.getLogger(RiftTest.class);

  public static void main(String... args) throws IOException, InterruptedException {
    OvrLibrary ovr = OvrLibrary.INSTANCE;
    ovr.ovr_Initialize();
    ovrHmd hmd = ovr.ovrHmd_Create(0);
    ovr.ovrHmd_StartSensor(hmd, 0, 0);
    for (int i = 0; i < 10; ++i) {
      ovrSensorState_ sensor = ovr.ovrHmd_GetSensorState(hmd, ovr.ovr_GetTimeInSeconds());
      LOG.debug(String.format("%f %f %f", 
          sensor.Recorded.LinearAcceleration.x,
          sensor.Recorded.LinearAcceleration.y,
          sensor.Recorded.LinearAcceleration.z));
      Thread.sleep(1000);
    }
    ovr.ovrHmd_StopSensor(hmd);
    ovr.ovrHmd_Destroy(hmd);
    ovr.ovr_Shutdown();
  }
}
