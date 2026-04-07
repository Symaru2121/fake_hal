#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace fake_hal {


class NoiseOverlay {
public:

    explicit NoiseOverlay(
        float readNoiseSigma = 3.0f,
        float shotNoiseFactor = 0.35f,
        uint64_t fpnSeed = 0xDEADBEEF42ULL
    );


    void apply(uint8_t* nv21, int width, int height, float isoGain = 1.0f);


    void applyLuma(uint8_t* yPlane, int width, int height, float isoGain = 1.0f);


    void setSensorFingerprint(const std::string& deviceSerial, const std::string& modelName);

private:
    float readNoiseSigma_;
    float shotNoiseFactor_;


    uint64_t fpnSeed_ = 0xFEEDFACE12345678ULL;


    std::vector<int8_t> fpnMap_;
    int fpnWidth_ = 0, fpnHeight_ = 0;


    std::vector<int8_t> rowFpnMap_;


    std::vector<int8_t> colFpnMap_;


    std::vector<int> hotPixels_;
    std::vector<int> deadPixels_;


    std::mt19937 rngY_;
    std::mt19937 rngUV_;

    void ensureFPN(int width, int height);
};

}
