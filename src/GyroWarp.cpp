#include "GyroWarp.h"

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <vector>

#define LOG_TAG "FakeHAL_GyroWarp"
#include <log/log.h>

namespace fake_hal {

static constexpr float DEG_PER_RAD = 180.0f / M_PI;
static constexpr float RAD_PER_DEG = M_PI / 180.0f;

GyroWarp::GyroWarp(const std::string& iioDevicePath,
                   int maxShiftPx, float maxAngleDeg)
    : iioDevicePath_(iioDevicePath)
    , maxShiftPx_(maxShiftPx)
    , maxAngleDeg_(maxAngleDeg)
{}

GyroWarp::~GyroWarp() {
    stop();
}

bool GyroWarp::openIIO() {


    auto tryOpen = [](const std::string& path) -> int {
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            ALOGW("GyroWarp: cannot open %s: %s", path.c_str(), strerror(errno));
        }
        return fd;
    };


    std::vector<std::string> iioBasePaths = {
        "/sys/bus/iio/devices/iio:device0",
        "/sys/bus/iio/devices/iio:device1",
        "/sys/bus/iio/devices/iio:device2",
        "/sys/bus/iio/devices/iio:device3",
    };

    for (const auto& base : iioBasePaths) {
        int gx = tryOpen(base + "/in_anglvel_x_raw");
        int gy = tryOpen(base + "/in_anglvel_y_raw");
        if (gx >= 0 && gy >= 0) {
            fdGyroX_ = gx;
            fdGyroY_ = gy;


            char scalePath[256];
            snprintf(scalePath, sizeof(scalePath), "%s/in_anglvel_scale", base.c_str());
            FILE* f = fopen(scalePath, "r");
            if (f) {
                fscanf(f, "%f", &gyroScale_);
                fclose(f);
            } else {
                gyroScale_ = 0.000266316f;
            }
            ALOGI("GyroWarp: opened gyroscope at %s, scale=%f", base.c_str(), gyroScale_);
            break;
        }
        if (gx >= 0) close(gx);
        if (gy >= 0) close(gy);
    }


    for (const auto& base : iioBasePaths) {
        int ax = tryOpen(base + "/in_accel_x_raw");
        int ay = tryOpen(base + "/in_accel_y_raw");
        int az = tryOpen(base + "/in_accel_z_raw");
        if (ax >= 0 && ay >= 0 && az >= 0) {
            fdAccelX_ = ax;
            fdAccelY_ = ay;
            fdAccelZ_ = az;

            char scalePath[256];
            snprintf(scalePath, sizeof(scalePath), "%s/in_accel_scale", base.c_str());
            FILE* f = fopen(scalePath, "r");
            if (f) {
                fscanf(f, "%f", &accelScale_);
                fclose(f);
            } else {
                accelScale_ = 0.000598144f;
            }
            ALOGI("GyroWarp: opened accelerometer at %s", base.c_str());
            break;
        }
        if (ax >= 0) close(ax);
        if (ay >= 0) close(ay);
        if (az >= 0) close(az);
    }

    return (fdGyroX_ >= 0 && fdGyroY_ >= 0);
}

float GyroWarp::readSysfs(int fd) {
    if (fd < 0) return 0.0f;
    char buf[32] = {};
    lseek(fd, 0, SEEK_SET);
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return 0.0f;
    return (float)atof(buf);
}

bool GyroWarp::start() {
    if (!openIIO()) {
        ALOGE("GyroWarp: failed to open IIO devices — running without gyro");

    }

    running_ = true;
    readerThread_ = std::thread(&GyroWarp::readerLoop, this);
    return true;
}

void GyroWarp::stop() {
    running_ = false;
    if (readerThread_.joinable()) readerThread_.join();
    if (fdGyroX_  >= 0) { close(fdGyroX_);  fdGyroX_  = -1; }
    if (fdGyroY_  >= 0) { close(fdGyroY_);  fdGyroY_  = -1; }
    if (fdAccelX_ >= 0) { close(fdAccelX_); fdAccelX_ = -1; }
    if (fdAccelY_ >= 0) { close(fdAccelY_); fdAccelY_ = -1; }
    if (fdAccelZ_ >= 0) { close(fdAccelZ_); fdAccelZ_ = -1; }
}

void GyroWarp::readerLoop() {
    using namespace std::chrono;
    auto lastTime = steady_clock::now();

    while (running_) {
        auto now = steady_clock::now();
        float dt = duration_cast<microseconds>(now - lastTime).count() / 1'000'000.0f;
        lastTime = now;
        if (dt <= 0.0f || dt > 0.1f) dt = 0.01f;


        float gx = readSysfs(fdGyroX_) * gyroScale_;
        float gy = readSysfs(fdGyroY_) * gyroScale_;


        float ax = readSysfs(fdAccelX_) * accelScale_;
        float ay = readSysfs(fdAccelY_) * accelScale_;
        float az = readSysfs(fdAccelZ_) * accelScale_;


        float accelPitch = atan2f(ax, sqrtf(ay*ay + az*az)) * DEG_PER_RAD;
        float accelRoll  = atan2f(ay, sqrtf(ax*ax + az*az)) * DEG_PER_RAD;


        filtPitch_ = 0.98f * (filtPitch_ + gx * RAD_PER_DEG * dt) + 0.02f * accelPitch;
        filtRoll_  = 0.98f * (filtRoll_  + gy * RAD_PER_DEG * dt) + 0.02f * accelRoll;


        filtPitch_ = std::clamp(filtPitch_, -maxAngleDeg_, maxAngleDeg_);
        filtRoll_  = std::clamp(filtRoll_,  -maxAngleDeg_, maxAngleDeg_);

        pitchDeg_.store(filtPitch_);
        rollDeg_.store(filtRoll_);


        std::this_thread::sleep_for(milliseconds(10));
    }
}


void GyroWarp::shiftNV21(const uint8_t* src, uint8_t* dst,
                          int width, int height, int dx, int dy)
{

    for (int y = 0; y < height; y++) {
        int srcY = y - dy;
        for (int x = 0; x < width; x++) {
            int srcX = x - dx;
            if (srcX >= 0 && srcX < width && srcY >= 0 && srcY < height) {
                dst[y * width + x] = src[srcY * width + srcX];
            } else {

                int clampX = std::clamp(srcX, 0, width - 1);
                int clampY = std::clamp(srcY, 0, height - 1);
                dst[y * width + x] = src[clampY * width + clampX];
            }
        }
    }


    int uvWidth  = width  / 2;
    int uvHeight = height / 2;
    int dxUV = dx / 2;
    int dyUV = dy / 2;

    const uint8_t* srcUV = src    + width * height;
    uint8_t*       dstUV = dst    + width * height;

    for (int y = 0; y < uvHeight; y++) {
        int srcY = y - dyUV;
        for (int x = 0; x < uvWidth; x++) {
            int srcX = x - dxUV;
            int clampX = std::clamp(srcX, 0, uvWidth  - 1);
            int clampY = std::clamp(srcY, 0, uvHeight - 1);

            dstUV[(y * uvWidth + x) * 2 + 0] = srcUV[(clampY * uvWidth + clampX) * 2 + 0];
            dstUV[(y * uvWidth + x) * 2 + 1] = srcUV[(clampY * uvWidth + clampX) * 2 + 1];
        }
    }
}

void GyroWarp::apply(uint8_t* nv21, int width, int height, uint8_t* tmpBuf)
{
    float pitch = pitchDeg_.load();
    float roll  = rollDeg_.load();


    int dx = (int)((roll  / maxAngleDeg_) * maxShiftPx_);
    int dy = (int)((pitch / maxAngleDeg_) * maxShiftPx_);


    if (std::abs(dx) < 1 && std::abs(dy) < 1) return;


    size_t bufSize = (size_t)(width * height * 3 / 2);
    std::memcpy(tmpBuf, nv21, bufSize);
    shiftNV21(tmpBuf, nv21, width, height, dx, dy);
}

}
