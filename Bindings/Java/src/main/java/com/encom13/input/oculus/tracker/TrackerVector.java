package com.encom13.input.oculus.tracker;

import java.nio.ByteBuffer;

/**
 * Encapsulates the integer vectors provided by the Oculus Rift.  These are either encoded as short values, or
 * as  n
 *  
 * @author bdavis@saintandreas.org
 *
 */
public final class TrackerVector {
    private static final int BITS = 21;
    private static final int MASK = 1 << (BITS - 1);

    // Sign extension trick.  The values are packed as 21 bit signed integers
    // So we need to extend the sign at bit 21 all to way out to bit 32 for 
    // a Java int.  See http://graphics.stanford.edu/~seander/bithacks.html#VariableSignExtend
    private static int extend(int x) {
        return (x ^ MASK) - MASK;
    }

    public final int x;
    public final int y;
    public final int z;

    public TrackerVector(ByteBuffer buffer) {
        this(buffer, true);
    }

    public TrackerVector(ByteBuffer buffer, boolean packed) {
        if (packed) {
            byte[] bs = new byte[8];
            buffer.get(bs);
            x = extend(((bs[0] & 0xff) << 13) | ((bs[1] & 0xff) << 5) | ((bs[2] & 0xf8) >> 3));
            y = extend(((bs[2] & 0x07) << 18) | ((bs[3] & 0xff) << 10) | ((bs[4] & 0xff) << 2) | ((bs[5] & 0xc0) >> 6));
            z = extend(((bs[5] & 0x3f) << 15) | ((bs[6] & 0xff) << 7) | ((bs[7] & 0xff) >> 1));
        } else {
            x = buffer.getShort();
            y = buffer.getShort();
            z = buffer.getShort();
        }
    }
    
    public String toString() {
        return "X: " + x + ", Y: " + y + ", Z: " + z;
    }
}