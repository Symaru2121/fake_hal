#pragma once

#include <aidl/android/hardware/camera/device/BnCameraDevice.h>
#include <aidl/android/hardware/camera/device/BnCameraDeviceSession.h>
#include <aidl/android/hardware/camera/device/ICameraDeviceCallback.h>
#include <aidl/android/hardware/camera/device/StreamConfiguration.h>
#include <aidl/android/hardware/camera/device/CaptureRequest.h>
#include <aidl/android/hardware/camera/device/CaptureResult.h>
#include <aidl/android/hardware/camera/device/HalStream.h>

#include <camera/CameraMetadata.h>
#include <vndk/hardware_buffer.h>

#include "VideoFrameReader.h"
#include "MetadataRandomizer.h"
#include "NoiseOverlay.h"
#include "GyroWarp.h"
#include "JpegEncoder.h"
#include "GrallocHelper.h"
#include "TimestampSync.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <memory>
#include <string>

using namespace aidl::android::hardware::camera::device;
using namespace aidl::android::hardware::camera::common;

namespace fake_hal {


class FakeCameraDeviceSession : public BnCameraDeviceSession {
public:
    FakeCameraDeviceSession(
        const std::string& cameraId,
        const std::string& videoFilePath,
        const std::shared_ptr<ICameraDeviceCallback>& callback
    );
    ~FakeCameraDeviceSession() override;


    ndk::ScopedAStatus constructDefaultRequestSettings(
        RequestTemplate type,
        ::aidl::android::hardware::camera::device::CameraMetadata* requestTemplate) override;

    ndk::ScopedAStatus configureStreams(
        const StreamConfiguration& requestedConfiguration,
        std::vector<HalStream>* halStreams) override;

    ndk::ScopedAStatus processCaptureRequest(
        const std::vector<CaptureRequest>& requests,
        const std::vector<BufferCache>& cachesToRemove,
        int32_t* numRequestProcessed) override;

    ndk::ScopedAStatus flush() override;
    ndk::ScopedAStatus close() override;

    ndk::ScopedAStatus getCaptureRequestMetadataQueue(
        ::aidl::android::hardware::common::fmq::MQDescriptor<int8_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>* desc) override;

    ndk::ScopedAStatus getCaptureResultMetadataQueue(
        ::aidl::android::hardware::common::fmq::MQDescriptor<int8_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>* desc) override;

    ndk::ScopedAStatus isReconfigurationRequired(
        const ::aidl::android::hardware::camera::device::CameraMetadata& oldSessionParams,
        const ::aidl::android::hardware::camera::device::CameraMetadata& newSessionParams,
        bool* reconfigurationRequired) override;

    ndk::ScopedAStatus signalStreamFlush(
        const std::vector<int32_t>& streamIds,
        int32_t streamConfigCounter) override;

    ndk::ScopedAStatus switchToOffline(
        const std::vector<int32_t>& streamsToKeep,
        CameraOfflineSessionInfo* offlineSessionInfo,
        std::shared_ptr<ICameraOfflineSession>* session) override;

    ndk::ScopedAStatus repeatingRequestEnd(
        int32_t frameNumber,
        const std::vector<int32_t>& streamIds) override;

private:
    std::string cameraId_;
    std::string videoFilePath_;
    std::shared_ptr<ICameraDeviceCallback> callback_;


    std::unique_ptr<VideoFrameReader> videoReader_;
    std::unique_ptr<MetadataRandomizer> metaRand_;
    std::unique_ptr<NoiseOverlay> noiseOverlay_;
    std::unique_ptr<GyroWarp> gyroWarp_;
    std::unique_ptr<JpegEncoder> jpegEncoder_;
    std::unique_ptr<TimestampSync> tsSync_;


    std::vector<Stream> activeStreams_;


    std::vector<uint8_t> yuvBuf_;
    std::vector<uint8_t> tmpBuf_;


    struct PendingRequest {
        CaptureRequest request;
        int64_t timestampNs;
    };
    std::queue<PendingRequest> requestQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::thread workerThread_;
    std::atomic<bool> workerRunning_{false};

    std::atomic<uint32_t> frameNumber_{0};
    std::atomic<bool> flushing_{false};

    void workerLoop();

    void processOneRequest(const PendingRequest& req);

    void fillYUVBuffer(uint32_t width, uint32_t height);


    bool writeYUVToBuffer(
        const buffer_handle_t& handle,
        const uint8_t* nv21, int width, int height
    );


    bool writeJPEGToBuffer(
        const buffer_handle_t& handle,
        const uint8_t* nv21, int width, int height,
        const MetadataRandomizer& meta
    );


    static android::CameraMetadata buildCameraCharacteristics(
        const std::string& cameraId
    );
};


class FakeCameraDevice : public BnCameraDevice {
public:
    FakeCameraDevice(const std::string& cameraId,
                     const std::string& videoFilePath);

    ndk::ScopedAStatus getCameraCharacteristics(
        ::aidl::android::hardware::camera::device::CameraMetadata* chars) override;

    ndk::ScopedAStatus getPhysicalCameraCharacteristics(
        const std::string& physicalCameraId,
        ::aidl::android::hardware::camera::device::CameraMetadata* chars) override;

    ndk::ScopedAStatus getResourceCost(CameraResourceCost* cost) override;

    ndk::ScopedAStatus isStreamCombinationSupported(
        const StreamConfiguration& config, bool* support) override;

    ndk::ScopedAStatus open(
        const std::shared_ptr<ICameraDeviceCallback>& callback,
        std::shared_ptr<ICameraDeviceSession>* session) override;

    ndk::ScopedAStatus openInjectionSession(
        const std::shared_ptr<ICameraDeviceCallback>& callback,
        std::shared_ptr<ICameraInjectionSession>* session) override;

    ndk::ScopedAStatus setTorchMode(bool on) override;
    ndk::ScopedAStatus turnOnTorchWithStrengthLevel(int32_t strength) override;
    ndk::ScopedAStatus getTorchStrengthLevel(int32_t* strength) override;

private:
    std::string cameraId_;
    std::string videoFilePath_;
    android::CameraMetadata characteristics_;
};

}
