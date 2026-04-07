#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace fake_hal {


class RollingShutter {
public:

    explicit RollingShutter(float rollingShutterSkewUs = 33000.0f,
                            float pixelsPerRadPerSec = 200.0f)
        : rollingShutterSkewUs_(rollingShutterSkewUs)
        , pixelsPerRadPerSec_(pixelsPerRadPerSec)
    {}


    void apply(uint8_t* nv21, int width, int height,
               uint8_t* tmpBuf, float gyroRateRadS)
    {
        if (std::abs(gyroRateRadS) < 1e-4f) return;

        size_t bufSize = (size_t)(width * height * 3 / 2);
        std::memcpy(tmpBuf, nv21, bufSize);

        float rowTimeUs = rollingShutterSkewUs_ / (float)height;


        for (int row = 0; row < height; row++) {
            float timeOffsetUs = row * rowTimeUs;
            float timeOffsetS  = timeOffsetUs * 1e-6f;
            float dx = gyroRateRadS * timeOffsetS * pixelsPerRadPerSec_;
            int dxi = (int)std::round(dx);

            for (int col = 0; col < width; col++) {
                int srcCol = col - dxi;
                srcCol = std::clamp(srcCol, 0, width - 1);
                nv21[row * width + col] = tmpBuf[row * width + srcCol];
            }
        }


        int uvWidth  = width / 2;
        int uvHeight = height / 2;
        const uint8_t* srcUV = tmpBuf + width * height;
        uint8_t*       dstUV = nv21   + width * height;

        for (int row = 0; row < uvHeight; row++) {
            float timeOffsetUs = (row * 2) * rowTimeUs;
            float timeOffsetS  = timeOffsetUs * 1e-6f;
            float dx = gyroRateRadS * timeOffsetS * pixelsPerRadPerSec_;
            int dxi = (int)std::round(dx) / 2;

            for (int col = 0; col < uvWidth; col++) {
                int srcCol = col - dxi;
                srcCol = std::clamp(srcCol, 0, uvWidth - 1);
                dstUV[(row * uvWidth + col) * 2 + 0] = srcUV[(row * uvWidth + srcCol) * 2 + 0];
                dstUV[(row * uvWidth + col) * 2 + 1] = srcUV[(row * uvWidth + srcCol) * 2 + 1];
            }
        }
    }

    float getSkewUs() const { return rollingShutterSkewUs_; }
    void  setSkewUs(float us) { rollingShutterSkewUs_ = us; }
    float getPixelsPerRadPerSec() const { return pixelsPerRadPerSec_; }


    static int64_t computeRealSkewNs(int height, float readoutTimeMsPerLine = 0.011f) {

        return (int64_t)((float)height * readoutTimeMsPerLine * 1'000'000.0f);
    }

private:
    float rollingShutterSkewUs_;
    float pixelsPerRadPerSec_;
};

}
