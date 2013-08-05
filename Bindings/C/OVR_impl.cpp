#include "OculusVR.h"

#include <vector>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <json/json.h>
#include <libudev.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>

class Rift;

typedef boost::shared_ptr<Rift> RiftPtr;
typedef std::vector<RiftPtr> RiftVector;
typedef boost::asio::posix::stream_descriptor Fd;
typedef boost::shared_ptr<Fd> FdPtr;
typedef boost::asio::streambuf Buffer;
typedef boost::asio::io_service Svc;
typedef boost::shared_ptr<Svc> SvcPtr;
typedef boost::asio::deadline_timer Timer;
typedef boost::shared_ptr<udev> UdevPtr;

template<typename T2, typename T1>
inline T2 lexical_cast2(const T1 &in) {
    T2 out;
    std::stringstream ss;
    ss << std::hex << in;
    ss >> out;
    return out;
}

long timems() {
    static timeval tv;
    int ms = -1;
    if (gettimeofday(&tv, NULL) == 0) {
        ms = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
    }
    return ms;
}

class RiftManager {
    friend class RiftHid;
    friend class HidDevice;
    RiftVector rifts;
    UdevPtr Udev;
    SvcPtr svc;
    Svc::work work;
    Timer timer;
    boost::mutex lock;
    boost::thread workerThread;

private:
    RiftManager();
    ~RiftManager();
    void run();
public:
    static RiftManager & get();
    RiftVector & getRifts();
    Rift & getRift(OVR_HANDLE device);
};

/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor Data
/////////////////////////////////////////////////////////////////////////////////////////////
void UnpackSensor(const uint8_t* buffer, OvrVector & v) {
    // Sign extending trick
    // from http://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend
    struct {
        int32_t x :21;
    } s;
    v.x = s.x = (buffer[0] << 13) | (buffer[1] << 5) | ((buffer[2] & 0xF8) >> 3);
    v.y = s.x = ((buffer[2] & 0x07) << 18) | (buffer[3] << 10) | (buffer[4] << 2) | ((buffer[5] & 0xC0) >> 6);
    v.z = s.x = ((buffer[5] & 0x3F) << 15) | (buffer[6] << 7) | (buffer[7] >> 1);
}

// Reported data is little-endian now
uint16_t DecodeUInt16(const uint8_t* buffer) {
    return (buffer[1] << 8) | buffer[0];
}

int16_t DecodeSInt16(const uint8_t* buffer) {
    return (buffer[1] << 8) | buffer[0];
}

uint32_t DecodeUInt32(const uint8_t* buffer) {
    return (buffer[0]) | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
}

float DecodeFloat(const uint8_t* buffer) {
    union {
        uint32_t U;
        float F;
    } u;
    u.U = DecodeUInt32(buffer);
    return u.F;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Handle tracker message
/////////////////////////////////////////////////////////////////////////////////////////////

static const OvrSensorMessage& DecodeTracker(const uint8_t * buffer, size_t size) {
    if (size < 62) {
        throw std::exception();
    }

    static OvrSensorMessage result;
    memset(&result, 0, sizeof(OvrSensorMessage));
    int offset = 1;
    result.SampleCount = buffer[offset];
    result.Timestamp = DecodeUInt16(buffer + 2);
    result.LastCommandID = DecodeUInt16(buffer + 4);
    result.Temperature = DecodeSInt16(buffer + 6);
    // Only unpack as many samples as there actually are
    size_t iterationCount = std::min((size_t) result.SampleCount, (size_t) 3);
    for (uint8_t i = 0; i < iterationCount; i++) {
        UnpackSensor(buffer + 8 + 16 * i, result.Samples[i].Accel);
        UnpackSensor(buffer + 16 + 16 * i, result.Samples[i].Gyro);
    }

    result.Mag.x = DecodeSInt16(buffer + 56);
    result.Mag.y = DecodeSInt16(buffer + 58);
    result.Mag.z = DecodeSInt16(buffer + 60);
    return result;
}

static const OvrSensorMessage& DecodeTracker(const std::vector<uint8_t> & buffer) {
    const uint8_t & item = buffer[0];
    return DecodeTracker(&item, buffer.size());
}

class Rift {
public:
    OVR_SENSOR_CALLBACK callback;

    Rift()
            : callback(0) {

    }

    virtual ~Rift() {

    }

};

//struct SimPacket {
//    int time;
//    std::vector<uint8_t> data;
//};
//
//class RiftSim : public Rift {
//public:
//    boost::circular_buffer<SimPacket> testData;
//    size_t index;
//    int lastTime;
//    int lastInterval;
//
//    virtual void run() {
//        while (!quit) {
//            if (index >= testData.size()) {
//                index = 0;
//            }
//            SimPacket & packet = testData.at(index++);
//            if (packet.time < lastTime) {
//                usleep(lastInterval * 1000);
//            } else {
//                lastInterval = packet.time - lastTime;
//                usleep(lastInterval * 1000);
//            }
//            if (callback) {
//                const OvrSensorMessage & message = DecodeTracker(packet.data);
//                (*callback)(&message);
//            }
//            lastTime = packet.time;
//        }
//    }
//
//    // hexstr [in] - string with hex encoded bytes
//    // bytes [out] - output in binary form
//    static void hex_to_bin(std::string const& hexstr, vector<uint8_t> & bytes) {
//        bytes.clear();
//        bytes.reserve(hexstr.size() / 2);
//        for (std::string::size_type i = 0; i < hexstr.size() / 2; ++i) {
//            std::istringstream iss(hexstr.substr(i * 2, 2));
//            unsigned int n;
//            iss >> std::hex >> n;
//            bytes.push_back(static_cast<unsigned char>(n));
//        }
//    }
//
//    RiftSim(char * file)
//            : index(0), lastTime(0xFFFFFFFFF), lastInterval(0) {
//        json_object* top = json_object_from_file(file);
//        array_list * al = json_object_get_array(top);
//        testData.set_capacity(al->length);
//        for (int i = 3; i < al->length; ++i) {
//            json_object* packet = (json_object*) al->array[i];
//
//            SimPacket simPacket;
//            simPacket.time = json_object_get_int(json_object_object_get(packet, "time"));
//            hex_to_bin(string(json_object_get_string(json_object_object_get(packet, "packet"))), simPacket.data);
//            testData.push_back(simPacket);
//        }
//
//        thread_ = boost::shared_ptr<thread>(new thread(boost::bind(&Rift::run, this)));
//    }
//};

struct HidDevice {
    std::string path;
    std::string devNode;
    std::string manufacturer;
    std::string product;
    std::string serial;
    uint16_t vendorId;
    uint16_t productId;

    HidDevice(const std::string & path)
            : path(path) {
        udev_device * hid_dev = udev_device_new_from_syspath(RiftManager::get().Udev.get(), path.c_str());
        devNode = udev_device_get_devnode(hid_dev);

        boost::shared_ptr<udev_device> usb_dev(
                udev_device_get_parent_with_subsystem_devtype(hid_dev, "usb", "usb_device"),
                std::ptr_fun(udev_device_unref));

        if (!usb_dev.get()) {
            throw std::exception();
        }

        manufacturer = udev_device_get_sysattr_value(usb_dev.get(), "manufacturer");
        product = udev_device_get_sysattr_value(usb_dev.get(), "product");
        serial = udev_device_get_sysattr_value(usb_dev.get(), "serial");
        std::string vendorIdStr = std::string("0x")
                + std::string(udev_device_get_sysattr_value(usb_dev.get(), "idVendor"));
        std::string productIdStr = std::string("0x")
                + std::string(udev_device_get_sysattr_value(usb_dev.get(), "idProduct"));
        vendorId = lexical_cast2<short>(vendorIdStr);
        productId = lexical_cast2<short>(productIdStr);
    }
};

class RiftHid : public Rift {
    friend class RiftManager;
    FdPtr fd;
    HidDevice device;
    Buffer readBuffer;

    RiftHid(const HidDevice & device)
            : device(device), readBuffer(62) {
        fd = FdPtr(
                new boost::asio::posix::stream_descriptor(*RiftManager::get().svc.get(),
                        open(device.path.c_str(), O_RDWR)));
    }

public:
    virtual ~RiftHid() {
        fd->close();
    }

    bool openDevice() {
        onTimer(boost::system::error_code());
        initializeRead();
        return true;
    }

    void onTimer(const boost::system::error_code& error) {
        Timer & timer = RiftManager::get().timer;
        // TODO send the keepalive
        static const size_t SensorKeepalivePacketSize = 5;
        static uint8_t SensorKeepaliveBuffer[SensorKeepalivePacketSize] = {
             8, // usage or page or whatever
             0, 0, // command id
             10000 & 0xFF, 10000 >> 8 // interval
        };
        SetFeatureReport(SensorKeepaliveBuffer, SensorKeepalivePacketSize);
        timer.expires_from_now(boost::posix_time::microseconds(1000 * 1000 * 3));
        timer.async_wait(boost::bind(&RiftHid::onTimer, this, _1));
    }

    void initializeRead() {
        boost::asio::async_read(*fd, readBuffer,
                boost::bind(&RiftHid::processReadResult, this, boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    }

    void processReadResult(const boost::system::error_code& error, std::size_t length) {
        if (error) {
            closeDeviceOnIOError();
            return;
        }
        if (length) {
            const unsigned char* p1 = boost::asio::buffer_cast<const unsigned char*>(readBuffer.data());
            readBuffer.consume(length);
            OVR_SENSOR_CALLBACK lastCallback = callback;
            // We've got data.
            if (lastCallback) {
                static OvrSensorMessage message;
                message = DecodeTracker(p1, length);
                (*lastCallback)(&message);
            }
        }
        initializeRead();
    }

    void closeDevice() {
        fd->close();
    }

    void closeDeviceOnIOError() {
        closeDevice();
    }

    bool SetFeatureReport(uint8_t* data, size_t length) {
        int res = ioctl(fd->native(), HIDIOCSFEATURE(length), data);
        return res >= 0;
    }

    bool GetFeatureReport(uint8_t* data, size_t length) {
        int res = ioctl(fd->native(), HIDIOCGFEATURE(length), data);
        return res >= 0;
    }
};

RiftManager::~RiftManager() {
    svc->stop();
    workerThread.join();
    // make sure Shutdown was called.
}

void RiftManager::run() {
}

RiftManager::RiftManager()
        : //
        Udev(udev_new(), std::ptr_fun(udev_unref)), //
        svc(new Svc()), //
        work(*svc), //
        timer(*svc), //
        workerThread(boost::bind(&RiftManager::run, this)) //
{
    // List all the HID devices
    boost::shared_ptr<udev_enumerate> enumerate(udev_enumerate_new(Udev.get()), std::ptr_fun(udev_enumerate_unref));
    udev_enumerate_add_match_subsystem(enumerate.get(), "hidraw");
    udev_enumerate_scan_devices(enumerate.get());

    udev_list_entry * entry;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate.get()))
    {
        std::string hid_path(udev_list_entry_get_name(entry));
        rifts.push_back(RiftPtr(new RiftHid(hid_path)));
    }
}

RiftManager & RiftManager::get() {
    static RiftManager INSTANCE;
    return INSTANCE;
}

RiftVector & RiftManager::getRifts() {
    return rifts;
}

Rift & RiftManager::getRift(OVR_HANDLE device) {
    RiftVector & rifts = getRifts();
    if (device > rifts.size()) {
        throw std::exception();
    }
    RiftPtr riftPtr = rifts.at(device - 1);
    return *riftPtr.get();
}

extern "C" OVR_HANDLE ovrOpenRiftRecording(char * recordingFile) {
    RiftVector & rifts = RiftManager::get().getRifts();
    rifts.push_back(RiftPtr(new RiftSim(recordingFile)));
    return rifts.size();
}

extern "C" OVR_SENSOR_CALLBACK ovrRegisterSampleHandler(OVR_HANDLE device, OVR_SENSOR_CALLBACK newCallback) {
    Rift & rift = RiftManager::get().getRift(device);
    OVR_SENSOR_CALLBACK result = rift.callback;
    rift.callback = newCallback;
    return result;
}

extern "C" OVR_HANDLE ovrOpenFirstAvailableRift() {
}

