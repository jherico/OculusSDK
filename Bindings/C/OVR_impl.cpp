#include "OculusVR.h"

#include <vector>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/circular_buffer.hpp>
#include <json/json.h>

using namespace std;
using namespace boost;

/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor Data
/////////////////////////////////////////////////////////////////////////////////////////////
void UnpackSensor(const uint8_t* buffer, int32_t* x, int32_t* y, int32_t* z) {
    // Sign extending trick
    // from http://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend
    struct {
        int32_t x :21;
    } s;
    *x = s.x = (buffer[0] << 13) | (buffer[1] << 5) | ((buffer[2] & 0xF8) >> 3);
    *y = s.x = ((buffer[2] & 0x07) << 18) | (buffer[3] << 10) | (buffer[4] << 2) | ((buffer[5] & 0xC0) >> 6);
    *z = s.x = ((buffer[5] & 0x3F) << 15) | (buffer[6] << 7) | (buffer[7] >> 1);
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


class Rift {
public:
    boost::shared_ptr<thread> thread_;
    OVR_SENSOR_CALLBACK callback;
    bool quit;
    virtual ~Rift() {

    }

    virtual void run() {

    }

    Rift() : callback(0), quit(false) {

    }

    /////////////////////////////////////////////////////////////////////////////////////////////
    // Handle tracker message
    /////////////////////////////////////////////////////////////////////////////////////////////
    static const OvrSensorMessage& DecodeTracker(const vector<uint8_t> & buffer) {
        return DecodeTracker(&buffer[0], buffer.size());
    }

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
            UnpackSensor(buffer + 8 + 16 * i, &result.Samples[i].AccelX, &result.Samples[i].AccelY,
                    &result.Samples[i].AccelZ);
            UnpackSensor(buffer + 16 + 16 * i, &result.Samples[i].GyroX, &result.Samples[i].GyroY,
                    &result.Samples[i].GyroZ);
        }

        result.MagX = DecodeSInt16(buffer + 56);
        result.MagY = DecodeSInt16(buffer + 58);
        result.MagZ = DecodeSInt16(buffer + 60);
        return result;
    }
};

struct SimPacket {
    int time;
    vector<uint8_t> data;
};

class RiftSim : public Rift {
public:
    circular_buffer<SimPacket> testData;
    size_t index;
    int lastTime;
    int lastInterval;

    virtual void run() {
        while (!quit) {
            if (index >= testData.size()) {
                index = 0;
            }
            SimPacket & packet = testData.at(index++);
            if (packet.time < lastTime) {
                usleep(lastInterval * 1000);
            } else {
                lastInterval = packet.time - lastTime;
                usleep(lastInterval * 1000);
            }
            if (callback) {
                const OvrSensorMessage & message = DecodeTracker(packet.data);
                (*callback)(&message);
            }
            lastTime = packet.time;
        }
    }

    // hexstr [in] - string with hex encoded bytes
    // bytes [out] - output in binary form
    static void hex_to_bin(std::string const& hexstr, vector<uint8_t> & bytes) {
        bytes.clear();
        bytes.reserve(hexstr.size() / 2);
        for (std::string::size_type i = 0; i < hexstr.size() / 2; ++i) {
            std::istringstream iss(hexstr.substr(i * 2, 2));
            unsigned int n;
            iss >> std::hex >> n;
            bytes.push_back(static_cast<unsigned char>(n));
        }
    }

    RiftSim(char * file) : index(0), lastTime(0xFFFFFFFFF), lastInterval(0) {
        json_object* top = json_object_from_file(file);
        array_list * al = json_object_get_array(top);
        testData.set_capacity(al->length);
        for (int i = 3; i < al->length; ++i) {
            json_object* packet = (json_object*) al->array[i];

            SimPacket simPacket;
            simPacket.time = json_object_get_int(json_object_object_get(packet, "time"));
            hex_to_bin(string(json_object_get_string(json_object_object_get(packet, "packet"))), simPacket.data);
            testData.push_back(simPacket);
        }

        thread_ = boost::shared_ptr<thread>(new thread(boost::bind(&Rift::run, this)));
    }
};

typedef boost::shared_ptr<Rift> RiftPtr;
typedef vector<RiftPtr> RiftVector;

RiftVector & getRifts() {
    static RiftVector rifts;
    return rifts;
}

OVR_HANDLE ovrOpenRiftRecording(char * recordingFile) {
    RiftVector & rifts = getRifts();
    rifts.push_back(RiftPtr(new RiftSim(recordingFile)));
    return rifts.size();
}

Rift & getRift(OVR_HANDLE device) {
    RiftVector & rifts = getRifts();
    if (device > rifts.size()) {
        throw std::exception();
    }
    RiftPtr riftPtr = rifts.at(device - 1);
    return *riftPtr.get();
}

OVR_SENSOR_CALLBACK ovrRegisterSampleHandler(OVR_HANDLE device, OVR_SENSOR_CALLBACK newCallback) {
    Rift & rift = getRift(device);
    OVR_SENSOR_CALLBACK result = rift.callback;
    rift.callback = newCallback;
    return result;
}

