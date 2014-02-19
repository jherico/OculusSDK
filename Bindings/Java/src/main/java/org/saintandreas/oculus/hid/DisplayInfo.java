package org.saintandreas.oculus.hid;

import java.io.IOException;
import java.nio.ByteBuffer;

import org.saintandreas.input.hid.HidFeatureReport;

import com.codeminders.hidapi.HIDDevice;

/**
 * DisplayInfo obtained from sensor; these values are used to report distortion
 * settings and other coefficients. Older SensorDisplayInfo will have all zeros
 * 
 */
public final class DisplayInfo extends HidFeatureReport {
    public static final byte  FEATURE_ID   = 9;
    public static final int   FEATURE_SIZE = 56;

    public static final float SCALE        = (1 / 1000000.f);
    public static final int   SIZE         = 56;

    public short              commandId;
    public byte               distortion;
    public short              xres, yres;
    public int                xsize, ysize;
    public int                center;
    public int                sep;
    public int[]              zeye;
    public float[]            distortionCoefficients;

    public DisplayInfo() {
        super(FEATURE_ID, FEATURE_SIZE);
    }

    public DisplayInfo(HIDDevice device) throws IOException {
        super(FEATURE_ID, FEATURE_SIZE, device);
    }

    @Override
    protected void parse(ByteBuffer buffer) {
        buffer.position(1);
        commandId = buffer.getShort();
        distortion = buffer.get();
        xres = buffer.getShort();
        yres = buffer.getShort();
        xsize = buffer.getInt();
        ysize = buffer.getInt();
        center = buffer.getInt();
        sep = buffer.getInt();
        zeye = new int[2];
        buffer.asIntBuffer().get(zeye);
        buffer.position(buffer.position() + 8);
        distortionCoefficients = new float[6];
        buffer.asFloatBuffer().get(distortionCoefficients);
        buffer.position(buffer.position() + 8);
    }

    @Override
    protected void pack(ByteBuffer buffer) {
        buffer.putShort(commandId);
        buffer.put(distortion);
        buffer.putShort(xres);
        buffer.putShort(yres);
        buffer.putInt(xsize);
        buffer.putInt(ysize);
        buffer.putInt(center);
        buffer.putInt(sep);
        for (int i = 0; i < 2; ++i) {
            buffer.putInt(zeye[i]);
        }
        for (int i = 0; i < 6; ++i) {
            buffer.putFloat(distortionCoefficients[i]);
        }
    }
}
