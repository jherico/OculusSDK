package com.oculusvr.rift.hid;

import java.io.IOException;
import java.nio.ByteBuffer;

import com.codeminders.hidapi.HIDDevice;
import com.oculusvr.input.hid.HidFeatureReport;

public final class SensorConfig extends HidFeatureReport {
    public static final byte FEATURE_ID = 2;
    public static final int FEATURE_SIZE = 7;

    public SensorConfig() {
        super(FEATURE_ID, FEATURE_SIZE);
    }

    public SensorConfig(HIDDevice device) throws IOException {
        super(FEATURE_ID, FEATURE_SIZE, device);
    }

    public enum Flag {
        RawMode(0x01), 
        CallibrationTest(0x02),
        UseCallibration(0x04), 
        AutoCallibration(0x08), 
        MotionKeepAlive(0x10), 
        CommandKeepAlive(0x20), 
        SensorCoordinates(0x40);

        public final int value;

        Flag(int value) {
            this.value = value;
        }
    };

    public short commandId;
    public byte flags;
    public short packetInterval;
    public short keepAliveInterval;

    @Override
    protected void parse(ByteBuffer buffer) {
        commandId = buffer.getShort();
        flags = buffer.get();
        packetInterval = buffer.get();
        keepAliveInterval = buffer.getShort();
    }

    @Override
    protected void pack(ByteBuffer buffer) {
        buffer.putShort(commandId);
        buffer.put(flags);
        buffer.put((byte) packetInterval);
        buffer.putShort(keepAliveInterval);
    }

}
