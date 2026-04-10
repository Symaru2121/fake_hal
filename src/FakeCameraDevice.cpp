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

#define LOG_TAG "FakeHAL_Device"
#include <log/log.h>

namespace fake_hal {


static android::CameraMetadata buildPixel7MainCharacteristics() {
    android::CameraMetadata meta;


    uint8_t facing = ANDROID_LENS_FACING_BACK;
    meta.update(ANDROID_LENS_FACING, &facing, 1);


    float physW = 8.64f, physH = 6.48f;
    meta.update(ANDROID_SENSOR_INFO_PHYSICAL_SIZE, new float[2]{physW, physH}, 2);


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


    int64_t expRange[2] = {14'000LL, 1'000'000'000LL};
    meta.update(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE, expRange, 2);


    int64_t frameDurRange[2] = {33'333'333LL, 200'000'000LL};
    meta.update(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, &frameDurRange[1], 1);


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
        minDurations.push_back(33'333'333LL);
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
        ANDROID_CONTROL_AF_MODE_OFF,
        ANDROID_CONTROL_AF_MODE_AUTO,
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO,
    };
    meta.update(ANDROID_CONTROL_AF_AVAILABLE_MODES, afModes, 4);


    uint8_t awbModes[] = {
        ANDROID_CONTROL_AWB_MODE_OFF,
        ANDROID_CONTROL_AWB_MODE_AUTO,
        ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
        ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
    };
    meta.update(ANDROID_CONTROL_AWB_AVAILABLE_MODES, awbModes, 4);


    uint8_t oisModes[] = {
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF,
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE_ON
    };
    meta.update(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION, oisModes, 2);


    uint8_t nrModes[] = {
        ANDROID_NOISE_REDUCTION_MODE_OFF,
        ANDROID_NOISE_REDUCTION_MODE_FAST,
        ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY,
    };
    meta.update(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES, nrModes, 3);


    float focusRange[2] = {0.0f, 10.0f};
    meta.update(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &focusRange[1], 1);


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


// ---- Pixel 4 (flame) camera characteristics ----
// Pixel 4 rear: Sony IMX363, 12.2 MP, f/1.7, 4.44mm focal, 1/2.55" sensor
static android::CameraMetadata buildPixel4MainCharacteristics() {
    android::CameraMetadata meta;

    uint8_t facing = ANDROID_LENS_FACING_BACK;
    meta.update(ANDROID_LENS_FACING, &facing, 1);

    // Sony IMX363: 1/2.55" sensor = ~5.64 x 4.23 mm
    float physW = 5.64f, physH = 4.23f;
    meta.update(ANDROID_SENSOR_INFO_PHYSICAL_SIZE, new float[2]{physW, physH}, 2);

    // 12.2 MP = 4032x3024
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

    int64_t expRange[2] = {13'000LL, 1'000'000'000LL};
    meta.update(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE, expRange, 2);

    int64_t frameDurRange[2] = {33'333'333LL, 200'000'000LL};
    meta.update(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, &frameDurRange[1], 1);

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

    meta.update(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                streamConfigs.data(), streamConfigs.size());

    std::vector<int64_t> minDurations;
    auto addDur = [&](int32_t fmt, int32_t w, int32_t h) {
        minDurations.push_back(fmt);
        minDurations.push_back(w);
        minDurations.push_back(h);
        minDurations.push_back(33'333'333LL);
    };
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 4032, 3024);
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 1920, 1080);
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 1280, 720);
    addDur(HAL_PIXEL_FORMAT_YCbCr_420_888, 640, 480);
    addDur(HAL_PIXEL_FORMAT_BLOB, 4032, 3024);
    addDur(HAL_PIXEL_FORMAT_BLOB, 1920, 1080);
    addDur(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 1920, 1080);
    addDur(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 1280, 720);

    meta.update(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS,
                minDurations.data(), minDurations.size());

    uint8_t aeModes[] = {ANDROID_CONTROL_AE_MODE_OFF, ANDROID_CONTROL_AE_MODE_ON};
    meta.update(ANDROID_CONTROL_AE_AVAILABLE_MODES, aeModes, 2);

    uint8_t afModes[] = {
        ANDROID_CONTROL_AF_MODE_OFF,
        ANDROID_CONTROL_AF_MODE_AUTO,
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE,
        ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO,
    };
    meta.update(ANDROID_CONTROL_AF_AVAILABLE_MODES, afModes, 4);

    uint8_t awbModes[] = {
        ANDROID_CONTROL_AWB_MODE_OFF,
        ANDROID_CONTROL_AWB_MODE_AUTO,
        ANDROID_CONTROL_AWB_MODE_DAYLIGHT,
        ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT,
    };
    meta.update(ANDROID_CONTROL_AWB_AVAILABLE_MODES, awbModes, 4);

    uint8_t oisModes[] = {
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF,
        ANDROID_LENS_OPTICAL_STABILIZATION_MODE_ON
    };
    meta.update(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION, oisModes, 2);

    uint8_t nrModes[] = {
        ANDROID_NOISE_REDUCTION_MODE_OFF,
        ANDROID_NOISE_REDUCTION_MODE_FAST,
        ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY,
    };
    meta.update(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES, nrModes, 3);

    float focusRange[2] = {0.0f, 10.0f};
    meta.update(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &focusRange[1], 1);

    uint8_t croppingType = ANDROID_SCALER_CROPPING_TYPE_CENTER_ONLY;
    meta.update(ANDROID_SCALER_CROPPING_TYPE, &croppingType, 1);

    float maxZoom = 8.0f;
    meta.update(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, &maxZoom, 1);

    return meta;
}

// Pixel 4 front: 8 MP, f/2.0, 3.0mm focal
static android::CameraMetadata buildPixel4FrontCharacteristics() {
    android::CameraMetadata meta = buildPixel4MainCharacteristics();

    uint8_t facing = ANDROID_LENS_FACING_FRONT;
    meta.update(ANDROID_LENS_FACING, &facing, 1);

    float aperture = 2.0f;
    meta.update(ANDROID_LENS_INFO_AVAILABLE_APERTURES, &aperture, 1);

    float focalLen = 3.0f;
    meta.update(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &focalLen, 1);

    // 8 MP = 3264x2448
    int32_t pixelArray[2] = {3264, 2448};
    meta.update(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, pixelArray, 2);

    int32_t activeArray[4] = {0, 0, 3264, 2448};
    meta.update(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, activeArray, 4);

    return meta;
}

// Determine device model at runtime (or default to Pixel 7)
static std::string getDeviceCodename() {
#ifdef __ANDROID__
    char prop[92] = {};
    if (__system_property_get("ro.product.device", prop) > 0) return prop;
#endif
    return "panther"; // default
}


FakeCameraDevice::FakeCameraDevice(const std::string& cameraId,
                                   const std::string& videoFilePath)
    : cameraId_(cameraId), videoFilePath_(videoFilePath)
{
    std::string codename = getDeviceCodename();
    bool isPixel4 = (codename == "flame" || codename == "coral");

    if (cameraId == "0") {
        characteristics_ = isPixel4 ? buildPixel4MainCharacteristics()
                                   : buildPixel7MainCharacteristics();
    } else {
        characteristics_ = isPixel4 ? buildPixel4FrontCharacteristics()
                                   : buildPixel7FrontCharacteristics();
    }
}

ndk::ScopedAStatus FakeCameraDevice::getCameraCharacteristics(
    android::hardware::camera::common::V1_0::helper::CameraMetadata* chars)
{
    *chars = characteristics_;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraDevice::getPhysicalCameraCharacteristics(
    const std::string&,
    android::hardware::camera::common::V1_0::helper::CameraMetadata*)
{
    return ndk::ScopedAStatus::fromServiceSpecificError(
        static_cast<int32_t>(Status::ILLEGAL_ARGUMENT));
}

ndk::ScopedAStatus FakeCameraDevice::getResourceCost(CameraResourceCost* cost) {
    cost->resourceCost = 50;
    cost->conflictingDevices.clear();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraDevice::isStreamCombinationSupported(
    const StreamConfiguration&, bool* support)
{
    *support = true;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraDevice::open(
    const std::shared_ptr<ICameraDeviceCallback>& callback,
    std::shared_ptr<ICameraDeviceSession>* session)
{
    ALOGI("FakeCameraDevice[%s]: open()", cameraId_.c_str());
    *session = ndk::SharedRefBase::make<FakeCameraDeviceSession>(
        cameraId_, videoFilePath_, callback);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraDevice::openInjectionSession(
    const std::shared_ptr<ICameraDeviceCallback>&,
    std::shared_ptr<ICameraInjectionSession>*)
{
    return ndk::ScopedAStatus::fromServiceSpecificError(
        static_cast<int32_t>(Status::OPERATION_NOT_SUPPORTED));
}

ndk::ScopedAStatus FakeCameraDevice::setTorchMode(bool) {
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus FakeCameraDevice::turnOnTorchWithStrengthLevel(int32_t) {
    return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus FakeCameraDevice::getTorchStrengthLevel(int32_t* lvl) {
    *lvl = 0;
    return ndk::ScopedAStatus::ok();
}


FakeCameraDeviceSession::FakeCameraDeviceSession(
    const std::string& cameraId,
    const std::string& videoFilePath,
    const std::shared_ptr<ICameraDeviceCallback>& callback)
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

    gyroWarp_->start();


    workerRunning_ = true;
    workerThread_ = std::thread(&FakeCameraDeviceSession::workerLoop, this);

    ALOGI("FakeCameraDeviceSession[%s]: created", cameraId_.c_str());
}

FakeCameraDeviceSession::~FakeCameraDeviceSession() {
    close();
}

ndk::ScopedAStatus FakeCameraDeviceSession::close() {
    workerRunning_ = false;
    queueCv_.notify_all();
    if (workerThread_.joinable()) workerThread_.join();
    gyroWarp_->stop();
    videoReader_->close();
    ALOGI("FakeCameraDeviceSession[%s]: closed", cameraId_.c_str());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraDeviceSession::configureStreams(
    const StreamConfiguration& config,
    std::vector<HalStream>* halStreams)
{
    activeStreams_ = config.streams;
    halStreams->clear();


    int targetW = 1920, targetH = 1080;
    for (const auto& s : config.streams) {
        if (s.format == HAL_PIXEL_FORMAT_YCbCr_420_888 ||
            s.format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
            targetW = s.width;
            targetH = s.height;
            break;
        }
    }

    if (videoReader_->isOpen()) videoReader_->close();
    if (!videoReader_->open(targetW, targetH)) {
        ALOGE("FakeCameraDeviceSession: failed to open video %s",
              videoFilePath_.c_str());

    }

    size_t nv21Size = (size_t)(targetW * targetH * 3 / 2);
    yuvBuf_.resize(nv21Size, 128);
    tmpBuf_.resize(nv21Size);


    for (const auto& s : config.streams) {
        HalStream hs;
        hs.id = s.id;
        hs.overrideFormat = s.format;

        if (s.format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
            hs.overrideFormat = HAL_PIXEL_FORMAT_YCbCr_420_888;
        }

        hs.producerUsage = static_cast<int64_t>(
            GRALLOC1_PRODUCER_USAGE_CAMERA | GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN);
        hs.consumerUsage = 0;
        hs.maxBuffers    = 4;
        hs.supportOffline = false;

        halStreams->push_back(hs);
    }

    ALOGI("FakeCameraDeviceSession[%s]: configureStreams -> %zu streams, target %dx%d",
          cameraId_.c_str(), config.streams.size(), targetW, targetH);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraDeviceSession::processCaptureRequest(
    const std::vector<CaptureRequest>& requests,
    const std::vector<BufferCache>&,
    int32_t* numRequestProcessed)
{
    if (flushing_) {
        *numRequestProcessed = 0;
        return ndk::ScopedAStatus::ok();
    }

    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        for (const auto& req : requests) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            int64_t nowNs = (int64_t)ts.tv_sec * 1'000'000'000LL + ts.tv_nsec;

            requestQueue_.push({req, nowNs});
        }
    }
    queueCv_.notify_one();
    *numRequestProcessed = (int32_t)requests.size();
    return ndk::ScopedAStatus::ok();
}

void FakeCameraDeviceSession::workerLoop() {
    while (workerRunning_) {
        PendingRequest req;
        {
            std::unique_lock<std::mutex> lk(queueMutex_);
            queueCv_.wait(lk, [this] {
                return !requestQueue_.empty() || !workerRunning_;
            });
            if (!workerRunning_) break;
            req = requestQueue_.front();
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
        rollingShutter.apply(yuvBuf_.data(), (int)width, (int)height,
                             tmpBuf_.data(), gyroRate);
    }
}


bool FakeCameraDeviceSession::writeYUVToBuffer(
    const buffer_handle_t& handle,
    const uint8_t* nv21, int width, int height)
{
    if (!handle || !nv21 || width <= 0 || height <= 0) {
        ALOGE("writeYUVToBuffer: invalid parameters");
        return false;
    }

    void* ptr = nullptr;
    if (!GrallocHelper::getInstance().lock(handle, width, height,
                                           GRALLOC_USAGE_SW_WRITE_OFTEN, &ptr)) {
        ALOGE("writeYUVToBuffer: gralloc lock failed (version: %s)",
              GrallocHelper::getInstance().versionString());
        return false;
    }

    size_t yuvSize = (size_t)(width * height * 3 / 2);
    memcpy(ptr, nv21, yuvSize);

    GrallocHelper::getInstance().unlock(handle);

    ALOGV("writeYUVToBuffer: wrote %zu bytes NV21 (%dx%d) via %s",
          yuvSize, width, height,
          GrallocHelper::getInstance().versionString());
    return true;
}


#ifndef CAMERA_BLOB_ID_JPEG
#define CAMERA_BLOB_ID_JPEG 0x00FF
#endif

bool FakeCameraDeviceSession::writeJPEGToBuffer(
    const buffer_handle_t& handle,
    const uint8_t* nv21, int width, int height,
    const MetadataRandomizer& meta)
{
    if (!handle || !nv21 || width <= 0 || height <= 0) {
        ALOGE("writeJPEGToBuffer: invalid parameters");
        return false;
    }


    int32_t blobBufSize = 0;
    for (const auto& s : activeStreams_) {
        if (s.format == HAL_PIXEL_FORMAT_BLOB) {
            blobBufSize = s.width;
            break;
        }
    }
    if (blobBufSize <= 0) {
        blobBufSize = (int32_t)(width * height * 3);
    }


    void* blobPtr = nullptr;
    if (!GrallocHelper::getInstance().lock(handle, blobBufSize, 1,
                                           GRALLOC_USAGE_SW_WRITE_OFTEN, &blobPtr)) {
        ALOGE("writeJPEGToBuffer: gralloc lock failed (version: %s)",
              GrallocHelper::getInstance().versionString());
        return false;
    }


    JpegEncoder::ExifData exifData;
    exifData.imageWidth  = width;
    exifData.imageHeight = height;
    exifData.iso         = (int)meta.getCurrentISO();
    exifData.exposureSec = meta.getCurrentExposureMs() / 1000.0f;
    exifData.fNumber     = 1.85f;
    exifData.focalLength = 6.81f;


    int jpegQuality = 95;

    std::vector<uint8_t> jpegData;
    if (!jpegEncoder_->encode(nv21, width, height, jpegQuality, exifData, jpegData)) {
        ALOGE("writeJPEGToBuffer: JPEG encoding failed");
        GrallocHelper::getInstance().unlock(handle);
        return false;
    }


    struct CameraBlob {
        uint32_t blobId;
        uint32_t blobSize;
    };

    if ((int64_t)jpegData.size() + (int64_t)sizeof(CameraBlob) > (int64_t)blobBufSize) {
        ALOGE("writeJPEGToBuffer: JPEG %zu bytes + trailer %zu > blob buffer %d",
              jpegData.size(), sizeof(CameraBlob), blobBufSize);
        GrallocHelper::getInstance().unlock(handle);
        return false;
    }


    uint8_t* buf = static_cast<uint8_t*>(blobPtr);
    memcpy(buf, jpegData.data(), jpegData.size());


    CameraBlob* trailer = reinterpret_cast<CameraBlob*>(
        buf + blobBufSize - sizeof(CameraBlob));
    trailer->blobId   = CAMERA_BLOB_ID_JPEG;
    trailer->blobSize = (uint32_t)jpegData.size();


    GrallocHelper::getInstance().unlock(handle);

    ALOGD("writeJPEGToBuffer: wrote %zu bytes JPEG to blob buffer %d via %s",
          jpegData.size(), blobBufSize,
          GrallocHelper::getInstance().versionString());
    return true;
}

void FakeCameraDeviceSession::processOneRequest(const PendingRequest& pending) {
    const CaptureRequest& req = pending.request;


    tsSync_->markFrameStart();
    int64_t timestampNs = tsSync_->getExposureStartNs();


    int width = 1920, height = 1080;
    for (const auto& s : activeStreams_) {
        if (s.format == HAL_PIXEL_FORMAT_YCbCr_420_888 ||
            s.format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
            width  = s.width;
            height = s.height;
            break;
        }
    }


    fillYUVBuffer((uint32_t)width, (uint32_t)height);


    android::CameraMetadata resultMeta;
    metaRand_->fill(frameNumber_, &resultMeta, timestampNs, height,
                    tsSync_->getActualFrameDurationNs());


    std::vector<StreamBuffer> outputBuffers;
    for (const auto& ob : req.outputBuffers) {
        StreamBuffer sb;
        sb.streamId = ob.streamId;
        sb.bufferId  = ob.bufferId;
        sb.status    = BufferStatus::OK;


        int32_t fmt = HAL_PIXEL_FORMAT_YCbCr_420_888;
        for (const auto& s : activeStreams_) {
            if (s.id == ob.streamId) {
                fmt = s.format;
                break;
            }
        }

        if (fmt == HAL_PIXEL_FORMAT_BLOB) {

            writeJPEGToBuffer(ob.buffer, yuvBuf_.data(), width, height, *metaRand_);
        } else {

            writeYUVToBuffer(ob.buffer, yuvBuf_.data(), width, height);
        }


        sb.releaseFence = ndk::ScopedFileDescriptor(-1);
        outputBuffers.push_back(std::move(sb));
    }


    CaptureResult result;
    result.frameNumber       = frameNumber_++;
    result.outputBuffers     = outputBuffers;
    result.inputBuffer.streamId = -1;
    result.partialResult     = 1;


    camera_metadata_t* rawMeta = resultMeta.release();
    result.result.metadata.assign(
        (uint8_t*)rawMeta,
        (uint8_t*)rawMeta + get_camera_metadata_size(rawMeta)
    );
    free_camera_metadata(rawMeta);


    if (callback_) {
        callback_->processCaptureResult({result});
    }
}


ndk::ScopedAStatus FakeCameraDeviceSession::constructDefaultRequestSettings(
    RequestTemplate type,
    android::hardware::camera::common::V1_0::helper::CameraMetadata* meta)
{
    (void)type;

    uint8_t aeMode = ANDROID_CONTROL_AE_MODE_ON;
    meta->update(ANDROID_CONTROL_AE_MODE, &aeMode, 1);
    uint8_t afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
    meta->update(ANDROID_CONTROL_AF_MODE, &afMode, 1);
    uint8_t awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
    meta->update(ANDROID_CONTROL_AWB_MODE, &awbMode, 1);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraDeviceSession::flush() {
    flushing_ = true;
    std::lock_guard<std::mutex> lk(queueMutex_);
    while (!requestQueue_.empty()) requestQueue_.pop();
    flushing_ = false;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraDeviceSession::getCaptureRequestMetadataQueue(
    MQDescriptor<int8_t, android::hardware::kSynchronizedReadWrite>*)
{ return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION); }

ndk::ScopedAStatus FakeCameraDeviceSession::getCaptureResultMetadataQueue(
    MQDescriptor<int8_t, android::hardware::kSynchronizedReadWrite>*)
{ return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION); }

ndk::ScopedAStatus FakeCameraDeviceSession::isReconfigurationRequired(
    const android::hardware::camera::common::V1_0::helper::CameraMetadata&,
    const android::hardware::camera::common::V1_0::helper::CameraMetadata&,
    bool* out)
{ *out = false; return ndk::ScopedAStatus::ok(); }

ndk::ScopedAStatus FakeCameraDeviceSession::signalStreamFlush(
    const std::vector<int32_t>&, int32_t)
{ return ndk::ScopedAStatus::ok(); }

ndk::ScopedAStatus FakeCameraDeviceSession::switchToOffline(
    const std::vector<int32_t>&,
    CameraOfflineSessionInfo*,
    std::shared_ptr<ICameraOfflineSession>*)
{
    return ndk::ScopedAStatus::fromServiceSpecificError(
        static_cast<int32_t>(Status::OPERATION_NOT_SUPPORTED));
}

ndk::ScopedAStatus FakeCameraDeviceSession::repeatingRequestEnd(
    int32_t, const std::vector<int32_t>&)
{ return ndk::ScopedAStatus::ok(); }

}
