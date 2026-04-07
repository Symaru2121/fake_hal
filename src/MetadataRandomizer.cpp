#include "MetadataRandomizer.h"
#include "RollingShutter.h"
#include <system/camera_metadata.h>
#include <camera/CameraMetadata.h>
#include <cmath>
#include <algorithm>


#include <hardware/camera3.h>

namespace fake_hal {

MetadataRandomizer::MetadataRandomizer()
    : rng_(std::random_device{}())
    , stdNorm_(0.0f, 1.0f)
    , uni_(0.0f, 1.0f)
{

    iso_        = kBaseISO;
    exposureMs_ = kBaseExpMs;
    aperture_   = 1.85f;


    gainR_  = 1.82f;
    gainGr_ = 1.00f;
    gainGb_ = 1.00f;
    gainB_  = 2.15f;

    focusDiopters_       = 0.1f;
    rollingShutterSkewNs_ = 33'000'000LL;
    frameDurationNs_     = 33'333'333LL;
}


float MetadataRandomizer::drift(float current, float mean,
                                float theta, float sigma)
{
    float noise = stdNorm_(rng_);
    return current + theta * (mean - current) + sigma * noise;
}

void MetadataRandomizer::advance() {

    iso_ = drift(iso_, kBaseISO, 0.05f, 3.0f);
    iso_ = std::clamp(iso_, kMinISO, kMaxISO);


    exposureMs_ = drift(exposureMs_, kBaseExpMs, 0.03f, 0.3f);
    exposureMs_ = std::clamp(exposureMs_, kMinExpMs, kMaxExpMs);


    gainR_  = drift(gainR_,  1.82f, 0.02f, 0.008f);
    gainGr_ = drift(gainGr_, 1.00f, 0.02f, 0.003f);
    gainGb_ = drift(gainGb_, 1.00f, 0.02f, 0.003f);
    gainB_  = drift(gainB_,  2.15f, 0.02f, 0.008f);

    gainR_  = std::clamp(gainR_,  1.4f, 2.5f);
    gainGr_ = std::clamp(gainGr_, 0.9f, 1.1f);
    gainGb_ = std::clamp(gainGb_, 0.9f, 1.1f);
    gainB_  = std::clamp(gainB_,  1.6f, 2.8f);


    focusDiopters_ = drift(focusDiopters_, 0.1f, 0.01f, 0.002f);
    focusDiopters_ = std::clamp(focusDiopters_, 0.0f, 10.0f);


    float skewBase = 33'000'000.0f;
    rollingShutterSkewNs_ = (int64_t)drift(
        (float)rollingShutterSkewNs_, skewBase, 0.05f, 200000.0f);


    frameDurationNs_ = 33'333'333LL + (int64_t)(stdNorm_(rng_) * 50000.0f);
}

void MetadataRandomizer::fill(uint32_t frameNumber,
                               android::CameraMetadata* meta,
                               int64_t timestampNs,
                               int frameHeight,
                               int64_t frameDurationNs)
{
    (void)frameNumber;

    meta->update(ANDROID_SENSOR_TIMESTAMP, &timestampNs, 1);


    if (frameDurationNs > 0) {
        meta->update(ANDROID_SENSOR_FRAME_DURATION, &frameDurationNs, 1);
    } else {
        meta->update(ANDROID_SENSOR_FRAME_DURATION, &frameDurationNs_, 1);
    }


    int64_t dynamicSkewNs = RollingShutter::computeRealSkewNs(frameHeight);

    dynamicSkewNs += (int64_t)(stdNorm_(rng_) * (float)dynamicSkewNs * 0.02f);
    meta->update(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW, &dynamicSkewNs, 1);


    int32_t isoInt = static_cast<int32_t>(iso_);
    meta->update(ANDROID_SENSOR_SENSITIVITY, &isoInt, 1);


    int64_t expNs = static_cast<int64_t>(exposureMs_ * 1'000'000.0f);
    meta->update(ANDROID_SENSOR_EXPOSURE_TIME, &expNs, 1);


    float gains[4] = { gainR_, gainGr_, gainGb_, gainB_ };
    meta->update(ANDROID_COLOR_CORRECTION_GAINS, gains, 4);


    camera_metadata_rational_t ccm[9] = {
        {(int32_t)(1024 * 1.0f + stdNorm_(rng_) * 2), 1024},
        {(int32_t)(1024 * -0.2f + stdNorm_(rng_) * 1), 1024},
        {(int32_t)(1024 * 0.1f + stdNorm_(rng_) * 1), 1024},

        {(int32_t)(1024 * -0.15f + stdNorm_(rng_) * 1), 1024},
        {(int32_t)(1024 * 1.05f + stdNorm_(rng_) * 2), 1024},
        {(int32_t)(1024 * 0.05f + stdNorm_(rng_) * 1), 1024},

        {(int32_t)(1024 * 0.05f + stdNorm_(rng_) * 1), 1024},
        {(int32_t)(1024 * -0.2f + stdNorm_(rng_) * 1), 1024},
        {(int32_t)(1024 * 1.1f + stdNorm_(rng_) * 2), 1024},
    };
    meta->update(ANDROID_COLOR_CORRECTION_TRANSFORM, ccm, 9);


    uint8_t aeState = (uni_(rng_) < 0.05f)
        ? ANDROID_CONTROL_AE_STATE_SEARCHING
        : ANDROID_CONTROL_AE_STATE_CONVERGED;
    meta->update(ANDROID_CONTROL_AE_STATE, &aeState, 1);


    uint8_t afState = (uni_(rng_) < 0.05f)
        ? ANDROID_CONTROL_AF_STATE_PASSIVE_SCAN
        : ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED;
    meta->update(ANDROID_CONTROL_AF_STATE, &afState, 1);


    uint8_t awbState = (uni_(rng_) < 0.05f)
        ? ANDROID_CONTROL_AWB_STATE_SEARCHING
        : ANDROID_CONTROL_AWB_STATE_CONVERGED;
    meta->update(ANDROID_CONTROL_AWB_STATE, &awbState, 1);


    meta->update(ANDROID_LENS_FOCUS_DISTANCE, &focusDiopters_, 1);


    meta->update(ANDROID_LENS_APERTURE, &aperture_, 1);


    float focalLen = 6.81f;
    meta->update(ANDROID_LENS_FOCAL_LENGTH, &focalLen, 1);


    uint8_t oisMode = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_ON;
    meta->update(ANDROID_LENS_OPTICAL_STABILIZATION_MODE, &oisMode, 1);


    uint8_t flashState = ANDROID_FLASH_STATE_UNAVAILABLE;
    meta->update(ANDROID_FLASH_STATE, &flashState, 1);


    int32_t pipelineDepth = 3;
    meta->update(ANDROID_REQUEST_PIPELINE_DEPTH, &pipelineDepth, 1);


    uint8_t lsShadingMode = ANDROID_SHADING_MODE_FAST;
    meta->update(ANDROID_SHADING_MODE, &lsShadingMode, 1);


    uint8_t nrMode = ANDROID_NOISE_REDUCTION_MODE_FAST;
    meta->update(ANDROID_NOISE_REDUCTION_MODE, &nrMode, 1);


    uint8_t edgeMode = ANDROID_EDGE_MODE_FAST;
    meta->update(ANDROID_EDGE_MODE, &edgeMode, 1);


    uint8_t hpMode = ANDROID_HOT_PIXEL_MODE_FAST;
    meta->update(ANDROID_HOT_PIXEL_MODE, &hpMode, 1);


    uint8_t lensState = ANDROID_LENS_STATE_STATIONARY;
    meta->update(ANDROID_LENS_STATE, &lensState, 1);


    advance();
}

}
