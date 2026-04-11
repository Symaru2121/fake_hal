#pragma once

#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <android/hardware/camera/device/3.2/ICameraDeviceSession.h>
#include <android/hardware/camera/device/3.2/ICameraDeviceCallback.h>
#include <android/hardware/camera/device/3.5/ICameraDevice.h>
#include <android/hardware/camera/device/3.7/ICameraDevice.h>
#include <android/hardware/camera/device/3.7/ICameraDeviceSession.h>
#include <android/hardware/camera/device/3.2/types.h>
#include <android/hardware/camera/device/3.4/types.h>
#include <android/hardware/camera/device/3.5/types.h>
#include <android/hardware/camera/device/3.7/types.h>
#include <android/hardware/camera/common/1.0/types.h>

#include <fmq/MessageQueue.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

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

namespace fake_hal {

using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::camera::common::V1_0::Status;
using ::android::hardware::camera::common::V1_0::TorchMode;

namespace V3_2 = ::android::hardware::camera::device::V3_2;
namespace V3_4 = ::android::hardware::camera::device::V3_4;
namespace V3_5 = ::android::hardware::camera::device::V3_5;
namespace V3_6 = ::android::hardware::camera::device::V3_6;
namespace V3_7 = ::android::hardware::camera::device::V3_7;

// MessageQueue types
using ::android::hardware::MessageQueue;
using ::android::hardware::kSynchronizedReadWrite;
using ResultMetadataQueue = MessageQueue<uint8_t, kSynchronizedReadWrite>;

/**
 * HIDL Camera Device Session implementing 3.7 interface (inherits 3.6->3.5->3.4->3.3->3.2)
 */
class FakeCameraDeviceSession : public V3_7::ICameraDeviceSession {
public:
    FakeCameraDeviceSession(
        const std::string& cameraId,
        const std::string& videoFilePath,
        const ::android::sp<V3_2::ICameraDeviceCallback>& callback
    );
    ~FakeCameraDeviceSession() override;

    // --- V3.2 methods ---
    Return<void> constructDefaultRequestSettings(
        V3_2::RequestTemplate type,
        constructDefaultRequestSettings_cb _hidl_cb) override;

    Return<void> configureStreams(
        const V3_2::StreamConfiguration& requestedConfiguration,
        configureStreams_cb _hidl_cb) override;

    Return<void> processCaptureRequest(
        const hidl_vec<V3_2::CaptureRequest>& requests,
        const hidl_vec<V3_2::BufferCache>& cachesToRemove,
        processCaptureRequest_cb _hidl_cb) override;

    Return<void> getCaptureRequestMetadataQueue(
        getCaptureRequestMetadataQueue_cb _hidl_cb) override;

    Return<void> getCaptureResultMetadataQueue(
        getCaptureResultMetadataQueue_cb _hidl_cb) override;

    Return<Status> flush() override;

    Return<void> close() override;

    // --- V3.3 methods ---
    Return<void> configureStreams_3_3(
        const V3_2::StreamConfiguration& requestedConfiguration,
        configureStreams_3_3_cb _hidl_cb) override;

    // --- V3.4 methods ---
    Return<void> configureStreams_3_4(
        const V3_4::StreamConfiguration& requestedConfiguration,
        configureStreams_3_4_cb _hidl_cb) override;

    Return<void> processCaptureRequest_3_4(
        const hidl_vec<V3_4::CaptureRequest>& requests,
        const hidl_vec<V3_2::BufferCache>& cachesToRemove,
        processCaptureRequest_3_4_cb _hidl_cb) override;

    // --- V3.5 methods ---
    Return<void> configureStreams_3_5(
        const V3_5::StreamConfiguration& requestedConfiguration,
        configureStreams_3_5_cb _hidl_cb) override;

    Return<void> signalStreamFlush(
        const hidl_vec<int32_t>& streamIds,
        uint32_t streamConfigCounter) override;

    Return<void> isReconfigurationRequired(
        const V3_2::CameraMetadata& oldSessionParams,
        const V3_2::CameraMetadata& newSessionParams,
        isReconfigurationRequired_cb _hidl_cb) override;

    // --- V3.6 methods ---
    Return<void> configureStreams_3_6(
        const V3_5::StreamConfiguration& requestedConfiguration,
        configureStreams_3_6_cb _hidl_cb) override;

    Return<void> switchToOffline(
        const hidl_vec<int32_t>& streamsToKeep,
        switchToOffline_cb _hidl_cb) override;

    // --- V3.7 methods ---
    Return<void> configureStreams_3_7(
        const V3_7::StreamConfiguration& requestedConfiguration,
        configureStreams_3_7_cb _hidl_cb) override;

    Return<void> processCaptureRequest_3_7(
        const hidl_vec<V3_7::CaptureRequest>& requests,
        const hidl_vec<V3_2::BufferCache>& cachesToRemove,
        processCaptureRequest_3_7_cb _hidl_cb) override;

private:
    std::string cameraId_;
    std::string videoFilePath_;
    ::android::sp<V3_2::ICameraDeviceCallback> callback_;

    std::unique_ptr<VideoFrameReader> videoReader_;
    std::unique_ptr<MetadataRandomizer> metaRand_;
    std::unique_ptr<NoiseOverlay> noiseOverlay_;
    std::unique_ptr<GyroWarp> gyroWarp_;
    std::unique_ptr<JpegEncoder> jpegEncoder_;
    std::unique_ptr<TimestampSync> tsSync_;

    // Active stream configuration (using 3.4 streams for richest info)
    std::vector<V3_4::Stream> activeStreams_;

    std::vector<uint8_t> yuvBuf_;
    std::vector<uint8_t> tmpBuf_;

    // FMQ for metadata
    std::unique_ptr<ResultMetadataQueue> requestMetadataQueue_;
    std::unique_ptr<ResultMetadataQueue> resultMetadataQueue_;

    struct PendingRequest {
        uint32_t frameNumber;
        V3_2::CameraMetadata settings;
        hidl_vec<V3_2::StreamBuffer> outputBuffers;
        int64_t timestampNs;
    };
    std::queue<PendingRequest> requestQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::thread workerThread_;
    std::atomic<bool> workerRunning_{false};

    std::atomic<uint32_t> frameCounter_{0};
    std::atomic<bool> flushing_{false};

    void workerLoop();
    void processOneRequest(const PendingRequest& req);
    void fillYUVBuffer(uint32_t width, uint32_t height);

    bool writeYUVToBuffer(
        const buffer_handle_t& handle,
        const uint8_t* nv21, int width, int height);

    bool writeJPEGToBuffer(
        const buffer_handle_t& handle,
        const uint8_t* nv21, int width, int height,
        const MetadataRandomizer& meta);

    // Helper: configure streams from a V3_4 stream list
    void doConfigureStreams(
        const hidl_vec<V3_4::Stream>& streams,
        std::vector<V3_4::HalStream>* halStreams);
};

/**
 * HIDL Camera Device implementing 3.7 interface
 */
class FakeCameraDevice : public V3_7::ICameraDevice {
public:
    FakeCameraDevice(const std::string& cameraId,
                     const std::string& videoFilePath);

    // --- V3.2 methods ---
    Return<void> getResourceCost(getResourceCost_cb _hidl_cb) override;

    Return<void> getCameraCharacteristics(
        getCameraCharacteristics_cb _hidl_cb) override;

    Return<Status> setTorchMode(TorchMode mode) override;

    Return<void> open(
        const ::android::sp<V3_2::ICameraDeviceCallback>& callback,
        open_cb _hidl_cb) override;

    Return<void> dumpState(const hidl_handle& fd) override;

    // --- V3.5 methods ---
    Return<void> getPhysicalCameraCharacteristics(
        const hidl_string& physicalCameraId,
        getPhysicalCameraCharacteristics_cb _hidl_cb) override;

    Return<void> isStreamCombinationSupported(
        const V3_4::StreamConfiguration& streams,
        isStreamCombinationSupported_cb _hidl_cb) override;

    // --- V3.7 methods ---
    Return<void> isStreamCombinationSupported_3_7(
        const V3_7::StreamConfiguration& streams,
        isStreamCombinationSupported_3_7_cb _hidl_cb) override;

private:
    std::string cameraId_;
    std::string videoFilePath_;
    android::CameraMetadata characteristics_;
};

} // namespace fake_hal
