#include "FakeCameraDevice.h"
#include "GrallocHelper.h"
#include "RollingShutter.h"
#include "LensShading.h"

#include <hardware/camera3.h>
#include <hardware/gralloc1.h>
#include <vndk/hardware_buffer.h>
#include <android/hardware_buffer.h>
#include <time.h>
#include <cstring>
#include <algorithm>
#include <sys/mman.h>

#ifndef FAKE_HAL_TEST_BUILD
#include <sys/system_properties.h>
#include <cutils/native_handle.h>
#endif

#undef LOG_TAG
#define LOG_TAG "FakeHAL_Device"
#include <log/log.h>

namespace fake_hal {

using PixelFormatHidl = ::android::hardware::graphics::common::V1_0::PixelFormat;
using BufferUsageHidl = ::android::hardware::graphics::common::V1_0::BufferUsage;

// ---- Pixel 7 (panther) camera characteristics ----
static android::CameraMetadata buildPixel7MainCharacteristics() {
    android::CameraMetadata meta;

    uint8_t facing = ANDROID_LENS_FACING_BACK;
    meta.update(ANDROID_LENS_FACING, &facing, 1);

    float physSize[2] = {8.64f, 6.48f};
    meta.update(ANDROID_SENSOR_INFO_PHYSICAL_SIZE, physSize, 2);

    int32_t pixelArray[2] = {4080, 3072};
    meta.update(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, pixelArray, 2);

    int32_t activeArray[4] = {0, 0, 4080, 3072};
    meta.update(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, activeArray, 4);

    float aperture = 1.85f;
    meta.update(ANDROID_LENS_INFO_AVAILABLE_APERTURES, &aperture, 1);

    float focalLen = 6.81f;
    meta.update(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &focalLen, 1);

    int32_t isoRange[2] = {50, 3200};
    meta.update(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE, isoRange, 2);

    int64_t expRange[2] = {14000LL, 1000000000LL};
    meta.update(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE, expRange, 2);

    int64_t maxFrameDur = 200000000LL;
    meta.update(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, &maxFrameDur, 1);

    uint8_t cfa = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_BGGR;
    meta.update(ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT, &cfa, 1);

    uint8_t hwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    meta.update(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL, &hwLevel, 1);

    uint8_t caps[] = {
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE,
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR,
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_POST_PROCESSING,
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES_READ_SENSOR_SETTINGS,
    };
    meta.update(ANDROID_REQUEST_AVAILABLE_CAPABILITIES, caps, sizeof(caps));

    uint8_t pipelineDepth = 4;
    meta.update(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, &pipelineDepth, 1);

    std::vector<int32_t> streamConfigs;
    auto addConfig = [&](int32_t fmt, int32_t w, int32_t h) {
        streamConfigs.push_back(fmt);
        streamConfigs.push_back(w);
        streamConfigs.push_back(h);
        streamConfigs.push_back(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT);
    };

    addConfig(HAL_PIXEL_FORMAT_BLOB, 4080, 3072);
    addConfig(HAL_PIXEL_FORMAT_BLOB, 1920, 1080);
    addConfig(HAL_PIXEL_FORMAT_YCbCr_420_888, 4080, 3072);
    addConfig(HAL_PIXEL_FORMAT_YCbCr_420_888, 1920, 1080);
    addConfig(HAL_PIXEL_FORMAT_YCbCr_420_888, 1280, 720);
    addConfig(HAL_PIXEL_FORMAT_YCbCr_420_888, 640, 480);
    addConfig(HAL_PIXEL_FORMAT_YCbCr_420_888, 320, 240);
    addConfig(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 1920, 1080);
    addConfig(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 1280, 720);
    addConfig(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 640, 480);

    meta.update(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                streamConfigs.data(), streamConfigs.size());

    std::vector<int64_t> minDurations;
    auto addDur = [&](int32_t fmt, int32_t w, int32_t h) {
        minDurations.push_back(fmt);
        minDurations.push_back(w);
        minDurations.push_back(h);
        minDurations.push_back(33333333LL);
    };
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 4080, 3072);
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 1920, 1080);
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 1280, 720);
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 640, 480);
    addDur(HAL_PIXEL_FORMAT_BLOB, 4080, 3072);
    addDur(HAL_PIXEL_FORMAT_BLOB, 1920, 1080);
    addDur(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 1920, 1080);
    addDur(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 1280, 720);

    meta.update(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
                minDurations.data(), minDurations.size());

    uint8_t aeModes[] = {ANDROID_CONTROL_AE_MODE_OFF, ANDROID_CONTROL_AE_MODE_ON};
    meta.update(ANDROID_CONTROL_AE_AVAILABLE_MODES, aeModes, 2);

    uint8_t afModes[] = {
        ANDROID_CONTROL_AF_MODE_OFF, ANDROID_CONTROL_AF_MODE_AUTO,
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE, ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO,
    };
    meta.update(ANDROID_CONTROL_AF_AVAILABLE_MODES, afModes, 4);

    uint8_t awbModes[] = {
        ANDROID_CONTROL_AWB_MODE_OFF, ANDROID_CONTROL_AWB_MODE_AUTO,
        ANDROID_CONTROL_AWB_MODE_DAYLIGHT, ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
    };
    meta.update(ANDROID_CONTROL_AWB_AVAILABLE_MODES, awbModes, 4);

    uint8_t oisModes[] = {
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF,
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE_ON
    };
    meta.update(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION, oisModes, 2);

    uint8_t nrModes[] = {
        ANDROID_NOISE_REDUCTION_MODE_OFF, ANDROID_NOISE_REDUCTION_MODE_FAST,
        ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY,
    };
    meta.update(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES, nrModes, 3);

    float minFocusDist = 10.0f;
    meta.update(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &minFocusDist, 1);

    uint8_t croppingType = ANDROID_SCALER_CROPPING_TYPE_CENTER_ONLY;
    meta.update(ANDROID_SCALER_CROPPING_TYPE, &croppingType, 1);

    float maxZoom = 8.0f;
    meta.update(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, &maxZoom, 1);

    return meta;
}

static android::CameraMetadata buildPixel7FrontCharacteristics() {
    android::CameraMetadata meta = buildPixel7MainCharacteristics();
    uint8_t facing = ANDROID_LENS_FACING_FRONT;
    meta.update(ANDROID_LENS_FACING, &facing, 1);
    float aperture = 2.2f;
    meta.update(ANDROID_LENS_INFO_AVAILABLE_APERTURES, &aperture, 1);
    float focalLen = 4.0f;
    meta.update(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &focalLen, 1);
    int32_t pixelArray[2] = {3840, 2880};
    meta.update(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, pixelArray, 2);
    return meta;
}

static android::CameraMetadata buildPixel4MainCharacteristics() {
    android::CameraMetadata meta;
    uint8_t facing = ANDROID_LENS_FACING_BACK;
    meta.update(ANDROID_LENS_FACING, &facing, 1);
    float physSize[2] = {5.64f, 4.23f};
    meta.update(ANDROID_SENSOR_INFO_PHYSICAL_SIZE, physSize, 2);
    int32_t pixelArray[2] = {4032, 3024};
    meta.update(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, pixelArray, 2);
    int32_t activeArray[4] = {0, 0, 4032, 3024};
    meta.update(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, activeArray, 4);
    float aperture = 1.7f;
    meta.update(ANDROID_LENS_INFO_AVAILABLE_APERTURES, &aperture, 1);
    float focalLen = 4.44f;
    meta.update(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &focalLen, 1);
    int32_t isoRange[2] = {50, 6400};
    meta.update(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE, isoRange, 2);
    int64_t expRange[2] = {13000LL, 1000000000LL};
    meta.update(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE, expRange, 2);
    int64_t maxFrameDur = 200000000LL;
    meta.update(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, &maxFrameDur, 1);
    uint8_t cfa = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
    meta.update(ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT, &cfa, 1);
    uint8_t hwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    meta.update(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL, &hwLevel, 1);
    uint8_t caps[] = {
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE,
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR,
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_POST_PROCESSING,
        ANDROID_REQUEST_AVAILABLE_CAPABILITIES_READ_SENSOR_SETTINGS,
    };
    meta.update(ANDROID_REQUEST_AVAILABLE_CAPABILITIES, caps, sizeof(caps));
    uint8_t pipelineDepth = 4;
    meta.update(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, &pipelineDepth, 1);

    std::vector<int32_t> streamConfigs;
    auto addConfig = [&](int32_t fmt, int32_t w, int32_t h) {
        streamConfigs.push_back(fmt);
        streamConfigs.push_back(w);
        streamConfigs.push_back(h);
        streamConfigs.push_back(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT);
    };
    addConfig(HAL_PIXEL_FORMAT_BLOB, 4032, 3024);
    addConfig(HAL_PIXEL_FORMAT_BLOB, 1920, 1080);
    addConfig(HAL_PIXEL_FORMAT_YCbCr_420_888, 4032, 3024);
    addConfig(HAL_PIXEL_FORMAT_YCbCr_420_888, 1920, 1080);
    addConfig(HAL_PIXEL_FORMAT_YCbCr_420_888, 1280, 720);
    addConfig(HAL_PIXEL_FORMAT_YCbCr_420_888, 640, 480);
    addConfig(HAL_PIXEL_FORMAT_YCbCr_420_888, 320, 240);
    addConfig(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 1920, 1080);
    addConfig(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 1280, 720);
    addConfig(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 640, 480);
    meta.update(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, streamConfigs.data(), streamConfigs.size());

    std::vector<int64_t> minDurations;
    auto addDur = [&](int32_t fmt, int32_t w, int32_t h) {
        minDurations.push_back(fmt);
        minDurations.push_back(w);
        minDurations.push_back(h);
        minDurations.push_back(33333333LL);
    };
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 4032, 3024);
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 1920, 1080);
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 1280, 720);
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 640, 480);
    addDur(HAL_PIXEL_FORMAT_BLOB, 4032, 3024);
    addDur(HAL_PIXEL_FORMAT_BLOB, 1920, 1080);
    addDur(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 1920, 1080);
    addDur(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 1280, 720);
    meta.update(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS, minDurations.data(), minDurations.size());

    uint8_t aeModes[] = {ANDROID_CONTROL_AE_MODE_OFF, ANDROID_CONTROL_AE_MODE_ON};
    meta.update(ANDROID_CONTROL_AE_AVAILABLE_MODES, aeModes, 2);
    uint8_t afModes[] = {ANDROID_CONTROL_AF_MODE_OFF, ANDROID_CONTROL_AF_MODE_AUTO,
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE, ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO};
    meta.update(ANDROID_CONTROL_AF_AVAILABLE_MODES, afModes, 4);
    uint8_t awbModes[] = {ANDROID_CONTROL_AWB_MODE_OFF, ANDROID_CONTROL_AWB_MODE_AUTO,
        ANDROID_CONTROL_AWB_MODE_DAYLIGHT, ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT};
    meta.update(ANDROID_CONTROL_AWB_AVAILABLE_MODES, awbModes, 4);
    uint8_t oisModes[] = {ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF, ANDROID_LENS_OPTICAL_STABILIZATION_MODE_ON};
    meta.update(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION, oisModes, 2);
    uint8_t nrModes[] = {ANDROID_NOISE_REDUCTION_MODE_OFF, ANDROID_NOISE_REDUCTION_MODE_FAST, ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY};
    meta.update(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES, nrModes, 3);
    float minFocusDist = 10.0f;
    meta.update(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &minFocusDist, 1);
    uint8_t croppingType = ANDROID_SCALER_CROPPING_TYPE_CENTER_ONLY;
    meta.update(ANDROID_SCALER_CROPPING_TYPE, &croppingType, 1);
    float maxZoom = 8.0f;
    meta.update(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, &maxZoom, 1);

    return meta;
}

static android::CameraMetadata buildPixel4FrontCharacteristics() {
    android::CameraMetadata meta = buildPixel4MainCharacteristics();
    uint8_t facing = ANDROID_LENS_FACING_FRONT;
    meta.update(ANDROID_LENS_FACING, &facing, 1);
    float aperture = 2.0f;
    meta.update(ANDROID_LENS_INFO_AVAILABLE_APERTURES, &aperture, 1);
    float focalLen = 3.0f;
    meta.update(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &focalLen, 1);
    int32_t pixelArray[2] = {3264, 2448};
    meta.update(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, pixelArray, 2);
    int32_t activeArray[4] = {0, 0, 3264, 2448};
    meta.update(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, activeArray, 4);
    return meta;
}

static std::string getDeviceCodename() {
#ifdef __ANDROID__
    char prop[92] = {};
    if (__system_property_get("ro.product.device", prop) > 0) return prop;
#endif
    return "panther";
}

// ===========================================================================
// FakeCameraDevice (HIDL 3.7)
// ===========================================================================

FakeCameraDevice::FakeCameraDevice(const std::string& cameraId,
                                   const std::string& videoFilePath)
    : cameraId_(cameraId), videoFilePath_(videoFilePath)
{
    std::string codename = getDeviceCodename();
    bool isPixel4 = (codename == "flame" || codename == "coral");
    if (cameraId == "0") {
        characteristics_ = isPixel4 ? buildPixel4MainCharacteristics() : buildPixel7MainCharacteristics();
    } else {
        characteristics_ = isPixel4 ? buildPixel4FrontCharacteristics() : buildPixel7FrontCharacteristics();
    }
}

Return<void> FakeCameraDevice::getResourceCost(getResourceCost_cb _hidl_cb) {
    ::android::hardware::camera::common::V1_0::CameraResourceCost cost;
    cost.resourceCost = 50;
    _hidl_cb(Status::OK, cost);
    return Void();
}

Return<void> FakeCameraDevice::getCameraCharacteristics(getCameraCharacteristics_cb _hidl_cb) {
    V3_2::CameraMetadata hidlMeta;
    camera_metadata_t* raw = characteristics_.release();
    if (raw) {
        size_t sz = get_camera_metadata_size(raw);
        hidlMeta.setToExternal((uint8_t*)raw, sz);
        _hidl_cb(Status::OK, hidlMeta);
        characteristics_.acquire(raw);
    } else {
        _hidl_cb(Status::INTERNAL_ERROR, hidlMeta);
    }
    return Void();
}

Return<Status> FakeCameraDevice::setTorchMode(TorchMode) {
    return Status::OK;
}

Return<void> FakeCameraDevice::open(
    const ::android::sp<V3_2::ICameraDeviceCallback>& callback,
    open_cb _hidl_cb)
{
    ALOGI("FakeCameraDevice[%s]: open()", cameraId_.c_str());
    ::android::sp<FakeCameraDeviceSession> session =
        new FakeCameraDeviceSession(cameraId_, videoFilePath_, callback);
    _hidl_cb(Status::OK, session);
    return Void();
}

Return<void> FakeCameraDevice::dumpState(const hidl_handle&) {
    return Void();
}

Return<void> FakeCameraDevice::getPhysicalCameraCharacteristics(
    const hidl_string&, getPhysicalCameraCharacteristics_cb _hidl_cb)
{
    V3_2::CameraMetadata emptyMeta;
    _hidl_cb(Status::ILLEGAL_ARGUMENT, emptyMeta);
    return Void();
}

Return<void> FakeCameraDevice::isStreamCombinationSupported(
    const V3_4::StreamConfiguration&, isStreamCombinationSupported_cb _hidl_cb)
{
    _hidl_cb(Status::OK, true);
    return Void();
}

Return<void> FakeCameraDevice::isStreamCombinationSupported_3_7(
    const V3_7::StreamConfiguration&, isStreamCombinationSupported_3_7_cb _hidl_cb)
{
    _hidl_cb(Status::OK, true);
    return Void();
}

// ===========================================================================
// FakeCameraDeviceSession (HIDL 3.7)
// ===========================================================================

FakeCameraDeviceSession::FakeCameraDeviceSession(
    const std::string& cameraId,
    const std::string& videoFilePath,
    const ::android::sp<V3_2::ICameraDeviceCallback>& callback)
    : cameraId_(cameraId)
    , videoFilePath_(videoFilePath)
    , callback_(callback)
{
    videoReader_ = std::make_unique<VideoFrameReader>(videoFilePath);
    metaRand_    = std::make_unique<MetadataRandomizer>();
    noiseOverlay_= std::make_unique<NoiseOverlay>(3.0f, 0.35f, 0xDEADBEEF42ULL);
    gyroWarp_    = std::make_unique<GyroWarp>();
    jpegEncoder_ = std::make_unique<JpegEncoder>();
    tsSync_      = std::make_unique<TimestampSync>();

    requestMetadataQueue_ = std::make_unique<ResultMetadataQueue>(1 << 20, false);
    resultMetadataQueue_  = std::make_unique<ResultMetadataQueue>(1 << 20, false);

    gyroWarp_->start();

    workerRunning_ = true;
    workerThread_ = std::thread(&FakeCameraDeviceSession::workerLoop, this);

    ALOGI("FakeCameraDeviceSession[%s]: created", cameraId_.c_str());
}

FakeCameraDeviceSession::~FakeCameraDeviceSession() {
    close();
}

Return<void> FakeCameraDeviceSession::close() {
    workerRunning_ = false;
    queueCv_.notify_all();
    if (workerThread_.joinable()) workerThread_.join();
    if (gyroWarp_) gyroWarp_->stop();
    if (videoReader_) videoReader_->close();
    ALOGI("FakeCameraDeviceSession[%s]: closed", cameraId_.c_str());
    return Void();
}

Return<void> FakeCameraDeviceSession::constructDefaultRequestSettings(
    V3_2::RequestTemplate type, constructDefaultRequestSettings_cb _hidl_cb)
{
    android::CameraMetadata settings;
    uint8_t controlMode = ANDROID_CONTROL_MODE_AUTO;
    settings.update(ANDROID_CONTROL_MODE, &controlMode, 1);
    uint8_t aeMode = ANDROID_CONTROL_AE_MODE_ON;
    settings.update(ANDROID_CONTROL_AE_MODE, &aeMode, 1);
    uint8_t awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
    settings.update(ANDROID_CONTROL_AWB_MODE, &awbMode, 1);

    uint8_t afMode;
    switch (type) {
        case V3_2::RequestTemplate::PREVIEW:
        case V3_2::RequestTemplate::VIDEO_RECORD:
            afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO; break;
        case V3_2::RequestTemplate::STILL_CAPTURE:
            afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE; break;
        default:
            afMode = ANDROID_CONTROL_AF_MODE_AUTO; break;
    }
    settings.update(ANDROID_CONTROL_AF_MODE, &afMode, 1);

    V3_2::CameraMetadata hidlMeta;
    camera_metadata_t* raw = settings.release();
    if (raw) {
        size_t sz = get_camera_metadata_size(raw);
        hidlMeta.setToExternal((uint8_t*)raw, sz);
        _hidl_cb(Status::OK, hidlMeta);
        free_camera_metadata(raw);
    } else {
        _hidl_cb(Status::INTERNAL_ERROR, hidlMeta);
    }
    return Void();
}

// Helper for configuring streams
void FakeCameraDeviceSession::doConfigureStreams(
    const hidl_vec<V3_4::Stream>& streams,
    std::vector<V3_4::HalStream>* halStreams)
{
    activeStreams_.clear();
    for (const auto& s : streams) {
        activeStreams_.push_back(s);
    }
    halStreams->clear();

    int targetW = 1920, targetH = 1080;
    for (const auto& s : streams) {
        if (s.v3_2.format == static_cast<PixelFormatHidl>(HAL_PIXEL_FORMAT_YCbCr_420_888) ||
            s.v3_2.format == static_cast<PixelFormatHidl>(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)) {
            targetW = s.v3_2.width;
            targetH = s.v3_2.height;
            break;
        }
    }

    if (videoReader_->isOpen()) videoReader_->close();
    if (!videoReader_->open(targetW, targetH)) {
        ALOGE("FakeCameraDeviceSession: failed to open video %s", videoFilePath_.c_str());
    }

    size_t nv21Size = (size_t)(targetW * targetH * 3 / 2);
    yuvBuf_.resize(nv21Size, 128);
    tmpBuf_.resize(nv21Size);

    for (const auto& s : streams) {
        V3_4::HalStream hs;
        hs.v3_3.v3_2.id = s.v3_2.id;
        hs.v3_3.v3_2.overrideFormat = s.v3_2.format;
        if (s.v3_2.format == static_cast<PixelFormatHidl>(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)) {
            hs.v3_3.v3_2.overrideFormat = static_cast<PixelFormatHidl>(HAL_PIXEL_FORMAT_YCbCr_420_888);
        }
        hs.v3_3.v3_2.producerUsage = static_cast<uint64_t>(
            GRALLOC1_PRODUCER_USAGE_CAMERA | GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN);
        hs.v3_3.v3_2.consumerUsage = 0;
        hs.v3_3.v3_2.maxBuffers = 4;
        hs.v3_3.overrideDataSpace = s.v3_2.dataSpace;
        hs.physicalCameraId = s.physicalCameraId;
        halStreams->push_back(hs);
    }

    ALOGI("FakeCameraDeviceSession[%s]: configureStreams -> %zu streams, target %dx%d",
          cameraId_.c_str(), streams.size(), targetW, targetH);
}

Return<void> FakeCameraDeviceSession::configureStreams(
    const V3_2::StreamConfiguration& config, configureStreams_cb _hidl_cb)
{
    hidl_vec<V3_4::Stream> streams34(config.streams.size());
    for (size_t i = 0; i < config.streams.size(); i++) {
        streams34[i].v3_2 = config.streams[i];
    }
    std::vector<V3_4::HalStream> halStreams34;
    doConfigureStreams(streams34, &halStreams34);

    V3_2::HalStreamConfiguration halConfig;
    halConfig.streams.resize(halStreams34.size());
    for (size_t i = 0; i < halStreams34.size(); i++) {
        halConfig.streams[i] = halStreams34[i].v3_3.v3_2;
    }
    _hidl_cb(Status::OK, halConfig);
    return Void();
}

Return<void> FakeCameraDeviceSession::configureStreams_3_3(
    const V3_2::StreamConfiguration& config, configureStreams_3_3_cb _hidl_cb)
{
    hidl_vec<V3_4::Stream> streams34(config.streams.size());
    for (size_t i = 0; i < config.streams.size(); i++) {
        streams34[i].v3_2 = config.streams[i];
    }
    std::vector<V3_4::HalStream> halStreams34;
    doConfigureStreams(streams34, &halStreams34);

    ::android::hardware::camera::device::V3_3::HalStreamConfiguration halConfig33;
    halConfig33.streams.resize(halStreams34.size());
    for (size_t i = 0; i < halStreams34.size(); i++) {
        halConfig33.streams[i] = halStreams34[i].v3_3;
    }
    _hidl_cb(Status::OK, halConfig33);
    return Void();
}

Return<void> FakeCameraDeviceSession::configureStreams_3_4(
    const V3_4::StreamConfiguration& config, configureStreams_3_4_cb _hidl_cb)
{
    std::vector<V3_4::HalStream> halStreams;
    doConfigureStreams(config.streams, &halStreams);
    V3_4::HalStreamConfiguration halConfig;
    halConfig.streams.resize(halStreams.size());
    for (size_t i = 0; i < halStreams.size(); i++) halConfig.streams[i] = halStreams[i];
    _hidl_cb(Status::OK, halConfig);
    return Void();
}

Return<void> FakeCameraDeviceSession::configureStreams_3_5(
    const V3_5::StreamConfiguration& config, configureStreams_3_5_cb _hidl_cb)
{
    std::vector<V3_4::HalStream> halStreams;
    doConfigureStreams(config.v3_4.streams, &halStreams);
    V3_4::HalStreamConfiguration halConfig;
    halConfig.streams.resize(halStreams.size());
    for (size_t i = 0; i < halStreams.size(); i++) halConfig.streams[i] = halStreams[i];
    _hidl_cb(Status::OK, halConfig);
    return Void();
}

Return<void> FakeCameraDeviceSession::configureStreams_3_6(
    const V3_5::StreamConfiguration& config, configureStreams_3_6_cb _hidl_cb)
{
    std::vector<V3_4::HalStream> halStreams;
    doConfigureStreams(config.v3_4.streams, &halStreams);
    V3_6::HalStreamConfiguration halConfig;
    halConfig.streams.resize(halStreams.size());
    for (size_t i = 0; i < halStreams.size(); i++) {
        halConfig.streams[i].v3_4 = halStreams[i];
        halConfig.streams[i].supportOffline = false;
    }
    _hidl_cb(Status::OK, halConfig);
    return Void();
}

Return<void> FakeCameraDeviceSession::configureStreams_3_7(
    const V3_7::StreamConfiguration& config, configureStreams_3_7_cb _hidl_cb)
{
    hidl_vec<V3_4::Stream> streams34(config.streams.size());
    for (size_t i = 0; i < config.streams.size(); i++) {
        streams34[i] = config.streams[i].v3_4;
    }
    std::vector<V3_4::HalStream> halStreams;
    doConfigureStreams(streams34, &halStreams);
    V3_6::HalStreamConfiguration halConfig;
    halConfig.streams.resize(halStreams.size());
    for (size_t i = 0; i < halStreams.size(); i++) {
        halConfig.streams[i].v3_4 = halStreams[i];
        halConfig.streams[i].supportOffline = false;
    }
    _hidl_cb(Status::OK, halConfig);
    return Void();
}

Return<void> FakeCameraDeviceSession::processCaptureRequest(
    const hidl_vec<V3_2::CaptureRequest>& requests,
    const hidl_vec<V3_2::BufferCache>&,
    processCaptureRequest_cb _hidl_cb)
{
    if (flushing_) { _hidl_cb(Status::OK, 0); return Void(); }
    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        for (const auto& req : requests) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            int64_t nowNs = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
            PendingRequest pending;
            pending.frameNumber = req.frameNumber;
            pending.settings = req.settings;
            pending.outputBuffers = req.outputBuffers;
            pending.timestampNs = nowNs;
            requestQueue_.push(std::move(pending));
        }
    }
    queueCv_.notify_one();
    _hidl_cb(Status::OK, (uint32_t)requests.size());
    return Void();
}

Return<void> FakeCameraDeviceSession::processCaptureRequest_3_4(
    const hidl_vec<V3_4::CaptureRequest>& requests,
    const hidl_vec<V3_2::BufferCache>&,
    processCaptureRequest_3_4_cb _hidl_cb)
{
    if (flushing_) { _hidl_cb(Status::OK, 0); return Void(); }
    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        for (const auto& req : requests) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            int64_t nowNs = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
            PendingRequest pending;
            pending.frameNumber = req.v3_2.frameNumber;
            pending.settings = req.v3_2.settings;
            pending.outputBuffers = req.v3_2.outputBuffers;
            pending.timestampNs = nowNs;
            requestQueue_.push(std::move(pending));
        }
    }
    queueCv_.notify_one();
    _hidl_cb(Status::OK, (uint32_t)requests.size());
    return Void();
}

Return<void> FakeCameraDeviceSession::processCaptureRequest_3_7(
    const hidl_vec<V3_7::CaptureRequest>& requests,
    const hidl_vec<V3_2::BufferCache>&,
    processCaptureRequest_3_7_cb _hidl_cb)
{
    if (flushing_) { _hidl_cb(Status::OK, 0); return Void(); }
    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        for (const auto& req : requests) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            int64_t nowNs = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
            PendingRequest pending;
            pending.frameNumber = req.v3_4.v3_2.frameNumber;
            pending.settings = req.v3_4.v3_2.settings;
            pending.outputBuffers = req.v3_4.v3_2.outputBuffers;
            pending.timestampNs = nowNs;
            requestQueue_.push(std::move(pending));
        }
    }
    queueCv_.notify_one();
    _hidl_cb(Status::OK, (uint32_t)requests.size());
    return Void();
}

Return<void> FakeCameraDeviceSession::getCaptureRequestMetadataQueue(
    getCaptureRequestMetadataQueue_cb _hidl_cb)
{
    _hidl_cb(*requestMetadataQueue_->getDesc());
    return Void();
}

Return<void> FakeCameraDeviceSession::getCaptureResultMetadataQueue(
    getCaptureResultMetadataQueue_cb _hidl_cb)
{
    _hidl_cb(*resultMetadataQueue_->getDesc());
    return Void();
}

Return<Status> FakeCameraDeviceSession::flush() {
    flushing_ = true;
    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        while (!requestQueue_.empty()) requestQueue_.pop();
    }
    flushing_ = false;
    return Status::OK;
}

Return<void> FakeCameraDeviceSession::signalStreamFlush(const hidl_vec<int32_t>&, uint32_t) {
    return Void();
}

Return<void> FakeCameraDeviceSession::isReconfigurationRequired(
    const V3_2::CameraMetadata&, const V3_2::CameraMetadata&,
    isReconfigurationRequired_cb _hidl_cb)
{
    _hidl_cb(Status::OK, true);
    return Void();
}

Return<void> FakeCameraDeviceSession::switchToOffline(
    const hidl_vec<int32_t>&, switchToOffline_cb _hidl_cb)
{
    V3_6::CameraOfflineSessionInfo info;
    _hidl_cb(Status::ILLEGAL_ARGUMENT, info, nullptr);
    return Void();
}

void FakeCameraDeviceSession::workerLoop() {
    while (workerRunning_) {
        PendingRequest req;
        {
            std::unique_lock<std::mutex> lk(queueMutex_);
            queueCv_.wait(lk, [this] { return !requestQueue_.empty() || !workerRunning_; });
            if (!workerRunning_) break;
            req = std::move(requestQueue_.front());
            requestQueue_.pop();
        }
        processOneRequest(req);
    }
}

void FakeCameraDeviceSession::fillYUVBuffer(uint32_t width, uint32_t height) {
    size_t needed = (size_t)(width * height * 3 / 2);
    if (yuvBuf_.size() != needed) yuvBuf_.resize(needed, 128);
    if (tmpBuf_.size() != needed) tmpBuf_.resize(needed);

    if (!videoReader_->nextFrame(yuvBuf_.data())) {
        ALOGW("FakeCameraDeviceSession: video reader returned false, using last frame");
        videoReader_->lastFrame(yuvBuf_.data());
    }

    gyroWarp_->apply(yuvBuf_.data(), (int)width, (int)height, tmpBuf_.data());
    float isoGain = metaRand_->getCurrentISO() / 100.0f;

    {
        static LensShading lensShading(0.4f);
        lensShading.apply(yuvBuf_.data(), (int)width, (int)height);
    }

    noiseOverlay_->apply(yuvBuf_.data(), (int)width, (int)height, isoGain);

    {
        static RollingShutter rollingShutter(33000.0f, 200.0f);
        float gyroRate = gyroWarp_ ? gyroWarp_->getRoll() * (M_PI / 180.0f) : 0.0f;
        rollingShutter.apply(yuvBuf_.data(), (int)width, (int)height, tmpBuf_.data(), gyroRate);
    }
}

bool FakeCameraDeviceSession::writeYUVToBuffer(
    const buffer_handle_t& handle, const uint8_t* nv21, int width, int height)
{
    if (!handle || !nv21 || width <= 0 || height <= 0) return false;
    void* ptr = nullptr;
    if (!GrallocHelper::getInstance().lock(handle, width, height, GRALLOC_USAGE_SW_WRITE_OFTEN, &ptr))
        return false;
    memcpy(ptr, nv21, (size_t)(width * height * 3 / 2));
    GrallocHelper::getInstance().unlock(handle);
    return true;
}

#ifndef CAMERA_BLOB_ID_JPEG
#define CAMERA_BLOB_ID_JPEG 0x00FF
#endif

bool FakeCameraDeviceSession::writeJPEGToBuffer(
    const buffer_handle_t& handle, const uint8_t* nv21, int width, int height,
    const MetadataRandomizer& meta)
{
    if (!handle || !nv21 || width <= 0 || height <= 0) return false;

    // HIDL V3.2 Stream doesn't have bufferSize field - estimate from dimensions
    int32_t blobBufSize = width * height * 3 / 2;

    JpegEncoder::ExifData exif;
    exif.iso = (int)meta.getCurrentISO();
    exif.exposureSec = meta.getCurrentExposureMs() / 1000.0f;
    exif.imageWidth = width;
    exif.imageHeight = height;

    std::vector<uint8_t> jpegData;
    if (!jpegEncoder_->encode(nv21, width, height, 95, exif, jpegData)) return false;

    void* ptr = nullptr;
    if (!GrallocHelper::getInstance().lock(handle, blobBufSize, 1, GRALLOC_USAGE_SW_WRITE_OFTEN, &ptr))
        return false;

    if ((int)jpegData.size() > blobBufSize - 16) {
        GrallocHelper::getInstance().unlock(handle);
        return false;
    }

    uint8_t* dst = (uint8_t*)ptr;
    memcpy(dst, jpegData.data(), jpegData.size());

    struct camera_jpeg_blob {
        uint16_t jpeg_blob_id;
        uint32_t jpeg_size;
    } __attribute__((packed));

    camera_jpeg_blob* blobHeader = reinterpret_cast<camera_jpeg_blob*>(
        dst + blobBufSize - sizeof(camera_jpeg_blob));
    blobHeader->jpeg_blob_id = CAMERA_BLOB_ID_JPEG;
    blobHeader->jpeg_size = jpegData.size();

    GrallocHelper::getInstance().unlock(handle);
    return true;
}

void FakeCameraDeviceSession::processOneRequest(const PendingRequest& req) {
    tsSync_->markFrameStart();
    metaRand_->advance();

    int targetW = 1920, targetH = 1080;
    for (const auto& s : activeStreams_) {
        if (s.v3_2.format == static_cast<PixelFormatHidl>(HAL_PIXEL_FORMAT_YCbCr_420_888) ||
            s.v3_2.format == static_cast<PixelFormatHidl>(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)) {
            targetW = s.v3_2.width;
            targetH = s.v3_2.height;
            break;
        }
    }

    fillYUVBuffer((uint32_t)targetW, (uint32_t)targetH);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t timestampNs = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;

    // Send shutter notification
    {
        hidl_vec<V3_2::NotifyMsg> msgs(1);
        msgs[0].type = V3_2::MsgType::SHUTTER;
        msgs[0].msg.shutter.frameNumber = req.frameNumber;
        msgs[0].msg.shutter.timestamp = (uint64_t)timestampNs;
        callback_->notify(msgs);
    }

    // Process each output buffer
    hidl_vec<V3_2::StreamBuffer> outputBuffers(req.outputBuffers.size());
    for (size_t i = 0; i < req.outputBuffers.size(); i++) {
        const auto& srcBuf = req.outputBuffers[i];
        auto& outBuf = outputBuffers[i];
        outBuf = srcBuf;

        int streamW = targetW, streamH = targetH;
        bool isJpeg = false;
        for (const auto& s : activeStreams_) {
            if (s.v3_2.id == srcBuf.streamId) {
                streamW = s.v3_2.width;
                streamH = s.v3_2.height;
                isJpeg = (s.v3_2.format == static_cast<PixelFormatHidl>(HAL_PIXEL_FORMAT_BLOB));
                break;
            }
        }

        buffer_handle_t handle = srcBuf.buffer.getNativeHandle();
        if (handle) {
            bool ok;
            if (isJpeg) {
                ok = writeJPEGToBuffer(handle, yuvBuf_.data(), streamW, streamH, *metaRand_);
            } else {
                ok = writeYUVToBuffer(handle, yuvBuf_.data(), streamW, streamH);
            }
            outBuf.status = ok ? V3_2::BufferStatus::OK : V3_2::BufferStatus::ERROR;
        } else {
            outBuf.status = V3_2::BufferStatus::ERROR;
        }
        outBuf.releaseFence = hidl_handle();
    }

    // Build result metadata
    android::CameraMetadata resultMeta;
    metaRand_->fill(req.frameNumber, &resultMeta, timestampNs, targetH);

    V3_2::CameraMetadata hidlResultMeta;
    camera_metadata_t* rawMeta = resultMeta.release();
    if (rawMeta) {
        size_t sz = get_camera_metadata_size(rawMeta);
        hidlResultMeta.setToExternal((uint8_t*)rawMeta, sz);
    }

    hidl_vec<V3_2::CaptureResult> results(1);
    results[0].frameNumber = req.frameNumber;
    results[0].result = hidlResultMeta;
    results[0].outputBuffers = outputBuffers;
    results[0].inputBuffer.streamId = -1;
    results[0].fmqResultSize = 0;
    results[0].partialResult = 1;

    callback_->processCaptureResult(results);

    if (rawMeta) {
        free_camera_metadata(rawMeta);
    }
}

} // namespace fake_hal
