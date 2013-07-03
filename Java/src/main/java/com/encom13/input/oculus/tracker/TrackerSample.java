package com.encom13.input.oculus.tracker;

import java.nio.ByteBuffer;

public final class TrackerSample {
    public final TrackerVector accel;
    public final TrackerVector gyro;

    public TrackerSample(ByteBuffer buffer) {
        accel = new TrackerVector(buffer);
        gyro = new TrackerVector(buffer);
    }
}