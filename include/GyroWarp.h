#pragma once

#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>

namespace fake_hal {


class GyroWarp {
public:

    explicit GyroWarp(
        const std::string& iioDevicePath = "/sys/bus/iio/devices/iio:device0",
        int maxShiftPx = 40,
        float maxAngleDeg = 20.0f
    );

    ~GyroWarp();


    bool start();
    void stop();


    void apply(uint8_t* nv21, int width, int height, uint8_t* tmpBuf);


    float getPitch() const { return pitchDeg_.load(); }
    float getRoll()  const { return rollDeg_.load();  }


    static void shiftNV21(
        const uint8_t* src, uint8_t* dst,
        int width, int height,
        int dx, int dy
    );

private:
    std::string iioDevicePath_;
    int maxShiftPx_;
    float maxAngleDeg_;


    std::atomic<float> pitchDeg_{0.0f};
    std::atomic<float> rollDeg_{0.0f};


    float filtPitch_ = 0.0f;
    float filtRoll_  = 0.0f;


    int fdGyroX_ = -1, fdGyroY_ = -1;
    int fdAccelX_ = -1, fdAccelY_ = -1, fdAccelZ_ = -1;
    float gyroScale_ = 1.0f;
    float accelScale_ = 1.0f;

    std::thread readerThread_;
    std::atomic<bool> running_{false};

    void readerLoop();

    bool openIIO();
    float readSysfs(int fd);
    float readAngle(int fd, float scale);

};

}
