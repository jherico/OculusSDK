package org.saintandreas.oculus.tracker;

import java.nio.ByteBuffer;
import java.util.List;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.Lists;

public class TrackerMessage {
    public final int                 type;
    public final int                 sampleCount;
    public final int                 timestamp;
    public final int                 lastCommandId;
    public final int                 temperature;
    public final List<TrackerSample> samples;
    public final TrackerVector       mag;

    public TrackerMessage(ByteBuffer buffer) {
        type = buffer.get();
        sampleCount = buffer.get() & 0xff;
        timestamp = (buffer.getShort() & 0xffff);
        lastCommandId = (buffer.getShort() & 0xffff);
        temperature = (buffer.getShort() & 0xffff);
        List<TrackerSample> samples = Lists.newArrayList();
        for (int i = 0; i < 3; ++i) {
            samples.add(new TrackerSample(buffer));
        }
        // this.samples = Lists.newArrayList();
        this.samples = ImmutableList.copyOf(samples.subList(0, Math.min(3, sampleCount)));
        mag = new TrackerVector(buffer, false);
    }
};
