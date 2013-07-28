package com.encom13.input.oculus;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import com.codeminders.hidapi.HIDDevice;
import com.codeminders.hidapi.HIDDeviceInfo;
import com.codeminders.hidapi.HIDManager;
import com.encom13.input.oculus.hid.DisplayInfo;
import com.encom13.input.oculus.hid.KeepAlive;
import com.encom13.input.oculus.hid.SensorConfig;
import com.encom13.input.oculus.hid.SensorRange;
import com.encom13.input.oculus.tracker.TrackerMessage;
import com.google.common.base.Predicate;

public final class RiftTracker implements Runnable {

    public static final int           SENSOR_VENDOR_ID    = 0x2833;
    public static final int           SENSOR_PRODUCT_ID   = 0x0001;
    private static final short        KEEP_ALIVE_TIME     = 10 * 1000;
    private static final short        KEEP_ALIVE_INTERVAL = 3 * 1000;

    // The device provided by the hidapi library
    private final HIDDevice           device;
    private final Thread              listener;
    private final ByteBuffer          buffer              = ByteBuffer.allocate(62).order(ByteOrder.nativeOrder());
    private volatile boolean          shutdown            = false;
    private Predicate<TrackerMessage> callback            = null;

    private RiftTracker(HIDDevice device) {
        this.device = device;
        this.listener = new Thread(this);
    }

    public void start(Predicate<TrackerMessage> callback) {
        this.callback = callback;
        this.listener.start();
    }

    public synchronized void stop() {
        if (listener.isAlive()) {
            shutdown = true;
            try {
                listener.join();
            } catch (InterruptedException e) {
                throw new IllegalStateException(e);
            }
            try {
                device.close();
            } catch (IOException e1) {
            }
        }
    }

    @Override
    public void run() {
        long nextKeepAlive = 0;
        int bytesRead = 0;
        try {
            device.enableBlocking();
            while (!shutdown) {
                if (System.currentTimeMillis() > nextKeepAlive) {
                    setKeepAliveMilliSeconds(KEEP_ALIVE_TIME);
                    nextKeepAlive = System.currentTimeMillis() + KEEP_ALIVE_INTERVAL;
                }
                int timeout = (int) Math.max(System.currentTimeMillis() - nextKeepAlive, 100);
                bytesRead = device.readTimeout(buffer.array(), timeout);
                if (0 == bytesRead) {
                    continue;
                }
                buffer.position(0);
                callback.apply(new TrackerMessage(buffer));
            }
        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    public DisplayInfo getDisplayInfo() throws IOException {
        DisplayInfo result = new DisplayInfo(device);
        return result;
    }

    public SensorRange getSensorRange() throws IOException {
        return new SensorRange(device);
    }

    public SensorConfig getSensorConfig() throws IOException {
        return new SensorConfig(device);
    }

    public void setSensorConfig(SensorConfig sc) throws IOException {
        sc.write(device);
    }

    public KeepAlive getKeepAlive() throws IOException {
        return new KeepAlive(device);
    }

    public void setKeepAlive(KeepAlive ka) throws IOException {
        ka.write(device);
    }

    public void setKeepAliveMilliSeconds(short milliseconds) throws IOException {
        this.setKeepAlive(new KeepAlive(milliseconds));
    }

    private static HIDDevice loadDevice() throws IOException {
        com.codeminders.hidapi.ClassPathLibraryLoader.loadNativeHIDLibrary();
        HIDManager mgr = HIDManager.getInstance();

        HIDDeviceInfo[] devs = mgr.listDevices();
        for (HIDDeviceInfo d : devs)
        {
            if (d.getVendor_id() == SENSOR_VENDOR_ID && d.getProduct_id() == SENSOR_PRODUCT_ID)
                return d.open();
        }

        return mgr.openById(SENSOR_VENDOR_ID, SENSOR_PRODUCT_ID, null);
    }

    private static RiftTracker INSTANCE = null;

    public synchronized static RiftTracker getInstance() throws IOException {
        if (null == INSTANCE) {
            INSTANCE = new RiftTracker(loadDevice());
        }
        return INSTANCE;
    }

    public static synchronized void startListening(Predicate<TrackerMessage> callback) throws IOException {
        getInstance().start(callback);
    }

    public static synchronized void stopListening() {
        if (null == INSTANCE) {
            return;
        }

        try {
            INSTANCE.stop();
        } finally {
            INSTANCE = null;
        }
    }
}
