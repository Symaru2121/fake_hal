#pragma once

#include <camera/CameraMetadata.h>
#include <system/camera_metadata.h>
#include <random>
#include <cstdint>

namespace fake_hal {


class MetadataRandomizer {
public:
    MetadataRandomizer();


    void fill(uint32_t frameNumber, android::CameraMetadata* meta, int64_t timestampNs,
              int frameHeight = 1080, int64_t frameDurationNs = 33'333'333LL);


    void advance();

private:

    float iso_;
    float exposureMs_;
    float aperture_;


    float gainR_, gainGr_, gainGb_, gainB_;


    float focusDiopters_;


    int64_t rollingShutterSkewNs_;
    int64_t frameDurationNs_;


    std::mt19937 rng_;
    std::normal_distribution<float> stdNorm_;
    std::uniform_real_distribution<float> uni_;


    float drift(float current, float mean, float theta, float sigma);


    static constexpr float kMinISO = 50.0f;
    static constexpr float kMaxISO = 3200.0f;
    static constexpr float kBaseISO = 100.0f;

    static constexpr float kMinExpMs = 0.5f;
    static constexpr float kMaxExpMs = 33.3f;
    static constexpr float kBaseExpMs = 16.0f;

public:

    float getCurrentISO() const { return iso_; }


    float getCurrentExposureMs() const { return exposureMs_; }
};

}
