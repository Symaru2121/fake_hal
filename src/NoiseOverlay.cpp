#include "NoiseOverlay.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace fake_hal {

NoiseOverlay::NoiseOverlay(float readNoiseSigma,
                           float shotNoiseFactor,
                           uint64_t fpnSeed)
    : readNoiseSigma_(readNoiseSigma)
    , shotNoiseFactor_(shotNoiseFactor)
    , fpnSeed_(fpnSeed)
    , rngY_((uint32_t)(fpnSeed ^ 0xAAAA5555ULL))
    , rngUV_((uint32_t)(fpnSeed ^ 0x5555AAAAULL))
{
}

void NoiseOverlay::setSensorFingerprint(const std::string& deviceSerial,
                                         const std::string& modelName) {


    uint64_t hash = 14695981039346656037ULL;
    auto hashBytes = [&](const std::string& s) {
        for (char c : s) {
            hash ^= (uint8_t)c;
            hash *= 1099511628211ULL;
        }
    };
    hashBytes(deviceSerial);
    hash ^= 0xDEADC0DEULL;
    hashBytes(modelName);

    fpnSeed_ = hash;

    fpnWidth_ = fpnHeight_ = 0;
    fpnMap_.clear();
    rowFpnMap_.clear();
    colFpnMap_.clear();
    hotPixels_.clear();
    deadPixels_.clear();
}

void NoiseOverlay::ensureFPN(int width, int height) {
    if (fpnWidth_ == width && fpnHeight_ == height) return;

    fpnWidth_  = width;
    fpnHeight_ = height;
    fpnMap_.resize(width * height);


    std::mt19937 fpnRng((uint32_t)fpnSeed_);
    std::normal_distribution<float> fpnDist(0.0f, 1.2f);

    for (int i = 0; i < width * height; i++) {
        int val = (int)fpnDist(fpnRng);
        fpnMap_[i] = (int8_t)std::clamp(val, -4, 4);
    }


    rowFpnMap_.resize(height);
    std::normal_distribution<float> rowDist(0.0f, 0.8f);
    for (int i = 0; i < height; i++)
        rowFpnMap_[i] = (int8_t)std::clamp((int)rowDist(fpnRng), -3, 3);


    colFpnMap_.resize(width);
    std::normal_distribution<float> colDist(0.0f, 0.4f);
    for (int i = 0; i < width; i++)
        colFpnMap_[i] = (int8_t)std::clamp((int)colDist(fpnRng), -2, 2);


    hotPixels_.clear();
    deadPixels_.clear();
    int numHot  = std::max(1, width * height / 10000);
    int numDead = std::max(1, width * height / 10000);
    std::uniform_int_distribution<int> posDist(0, width * height - 1);
    for (int i = 0; i < numHot;  i++) hotPixels_.push_back(posDist(fpnRng));
    for (int i = 0; i < numDead; i++) deadPixels_.push_back(posDist(fpnRng));
}

void NoiseOverlay::applyLuma(uint8_t* yPlane, int width, int height,
                             float isoGain)
{
    ensureFPN(width, height);


    float isoScale = std::sqrt(isoGain);

    std::normal_distribution<float> distY(0.0f, 1.0f);

    for (int i = 0; i < width * height; i++) {
        uint8_t L = yPlane[i];


        float luma = L / 255.0f;


        float sigma = std::sqrt(
            readNoiseSigma_ * readNoiseSigma_ * isoScale * isoScale
            + shotNoiseFactor_ * luma * 255.0f * isoScale
        );


        float temporalNoise = distY(rngY_) * sigma;


        int y = i / width;
        int x = i % width;
        float fpn = fpnMap_[i] * 0.4f
                  + rowFpnMap_[y] * 0.6f
                  + colFpnMap_[x] * 0.3f;

        int newVal = (int)(L + temporalNoise + fpn);
        yPlane[i] = (uint8_t)std::clamp(newVal, 0, 255);
    }


    int wh = width * height;
    for (int idx : hotPixels_)  if (idx < wh) yPlane[idx] = 255;
    for (int idx : deadPixels_) if (idx < wh) yPlane[idx] = 0;
}

void NoiseOverlay::apply(uint8_t* nv21, int width, int height,
                         float isoGain)
{

    applyLuma(nv21, width, height, isoGain);


    uint8_t* uvPlane = nv21 + width * height;
    int uvSize = width * height / 2;

    float uvSigma = readNoiseSigma_ * 0.5f * std::sqrt(isoGain);
    std::normal_distribution<float> distUV(0.0f, uvSigma);

    for (int i = 0; i < uvSize; i++) {
        int noise = (int)distUV(rngUV_);
        int newVal = (int)uvPlane[i] + noise;
        uvPlane[i] = (uint8_t)std::clamp(newVal, 0, 255);
    }
}

}
