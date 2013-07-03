package com.encom13.input.oculus.hid;

import java.io.IOException;
import java.nio.ByteBuffer;

import com.codeminders.hidapi.HIDDevice;
import com.encom13.input.hid.HidFeatureReport;

/**
 * Factor in the range conversion
 * 
 */
public final class SensorRange extends HidFeatureReport {
    public static final byte FEATURE_ID = 4;
    public static final int FEATURE_SIZE = 8;

    public static final float EARTH_GEES_TO_METERS_PER_SECOND = 9.81f;
    public static final float MILLIGAUSS_TO_GAUSS = 0.001f;

    public short commandId;
    // Meters per second squared
    public short accelerationScale;
    // Radians
    public short gyroScale;
    // Gauss
    public short magScale;

    public SensorRange() {
        super(FEATURE_ID, FEATURE_SIZE);
    }

    public SensorRange(HIDDevice device) throws IOException {
        super(FEATURE_ID, FEATURE_SIZE, device);
    }

    public void parse(ByteBuffer buffer) {
        commandId = buffer.getShort();
        accelerationScale = buffer.get();
        gyroScale = buffer.getShort();
        magScale = buffer.getShort();
    }

    public void pack(ByteBuffer buffer) {
        buffer.putShort(commandId);
        buffer.put((byte) accelerationScale);
        buffer.putShort(gyroScale);
        buffer.putShort(magScale);
    }
}