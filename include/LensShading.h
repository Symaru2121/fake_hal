#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace fake_hal {


class LensShading {
public:

    explicit LensShading(float k = 0.4f) : k_(k) {}


    void apply(uint8_t* nv21, int width, int height) {
        float cx = width  * 0.5f;
        float cy = height * 0.5f;

        float maxDist = std::sqrt(cx * cx + cy * cy);
        float invMaxDist = 1.0f / maxDist;

        for (int row = 0; row < height; row++) {
            float dy = row - cy;
            float dy2 = dy * dy;
            for (int col = 0; col < width; col++) {
                float dxf = col - cx;
                float r = std::sqrt(dxf * dxf + dy2) * invMaxDist;
                float gain = 1.0f / (1.0f + k_ * r * r);

                int idx = row * width + col;
                int val = (int)(nv21[idx] * gain);
                nv21[idx] = (uint8_t)std::clamp(val, 0, 255);
            }
        }
    }


    float gainAt(float r) const {
        return 1.0f / (1.0f + k_ * r * r);
    }

    float getK() const { return k_; }
    void  setK(float k) { k_ = k; }

private:
    float k_;
};

}
