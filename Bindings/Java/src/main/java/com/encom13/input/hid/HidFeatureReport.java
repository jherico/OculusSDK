/*
 * Copyright (c) 2013 Bradley Austin Davis <bdavis@saintandreas.org>. All rights
 * reserved.
 * 
 * Redistribution and use in source and binary forms are permitted provided that
 * the above copyright notice and this paragraph are duplicated in all such
 * forms and that any documentation, advertising materials, and other materials
 * related to such distribution and use acknowledge that the software was
 * developed by the <organization>. The name of the <organization> may not be
 * used to endorse or promote products derived from this software without
 * specific prior written permission. THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

package com.encom13.input.hid;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import com.codeminders.hidapi.HIDDevice;

/**
 * Encapsulate a feature report from HID. Derived classes must know their size
 * and feature ID as well as how to serialize to and from a bytebuffer
 * 
 */
public abstract class HidFeatureReport {
    public final byte featureId;
    public final int  size;

    public HidFeatureReport(byte featureId, int size) {
        this.featureId = featureId;
        this.size = size;
    }

    public HidFeatureReport(byte featureId, int size, HIDDevice device) throws IOException {
        this.featureId = featureId;
        this.size = size;
        read(device);
    }

    public final byte getFeatureId() {
        return featureId;
    }

    public final int getSize() {
        return size;
    }

    private ByteBuffer allocate() {
        ByteBuffer result = ByteBuffer.allocate(getSize()).order(ByteOrder.LITTLE_ENDIAN);
        result.put(getFeatureId());
        return result;
    }

    protected abstract void parse(ByteBuffer buffer);

    protected abstract void pack(ByteBuffer buffer);

    public void read(HIDDevice device) throws IOException {
        ByteBuffer buffer = allocate();
        device.getFeatureReport(buffer.array());
        buffer.position(0);
        if (this.featureId != buffer.get()) {
            throw new IllegalStateException();
        }
        parse(buffer);
    }

    public void write(HIDDevice device) throws IOException {
        ByteBuffer buffer = allocate();
        pack(buffer);
        device.sendFeatureReport(buffer.array());
    }
}