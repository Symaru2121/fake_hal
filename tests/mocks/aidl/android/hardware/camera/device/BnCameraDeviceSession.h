#pragma once

#include <memory>
#include <vector>
#include <string>
#include <aidl/android/hardware/camera/device/ndk_types.h>
#include <aidl/android/hardware/camera/device/StreamConfiguration.h>
#include <aidl/android/hardware/camera/device/CaptureRequest.h>
#include <aidl/android/hardware/camera/device/CaptureResult.h>
#include <aidl/android/hardware/camera/device/HalStream.h>
#include <aidl/android/hardware/camera/device/ICameraDeviceCallback.h>
#include <aidl/android/hardware/common/fmq/MQDescriptor.h>
#include <camera/CameraMetadata.h>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {


class ICameraOfflineSession {
public:
    virtual ~ICameraOfflineSession() = default;
};

struct CameraOfflineSessionInfo {

};


class ICameraDeviceSession {
public:
    virtual ~ICameraDeviceSession() = default;

    virtual ndk::ScopedAStatus configureStreams(
        const StreamConfiguration& config,
        std::vector<HalStream>* halStreams) = 0;

    virtual ndk::ScopedAStatus constructDefaultRequestSettings(
        RequestTemplate type,
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata* meta) = 0;

    virtual ndk::ScopedAStatus processCaptureRequest(
        const std::vector<CaptureRequest>& requests,
        const std::vector<BufferCache>& cachesToRemove,
        int32_t* numRequestProcessed) = 0;

    virtual ndk::ScopedAStatus flush() = 0;
    virtual ndk::ScopedAStatus close() = 0;

    virtual ndk::ScopedAStatus getCaptureRequestMetadataQueue(
        ::MQDescriptor<int8_t, ::android::hardware::kSynchronizedReadWrite>* desc) = 0;

    virtual ndk::ScopedAStatus getCaptureResultMetadataQueue(
        ::MQDescriptor<int8_t, ::android::hardware::kSynchronizedReadWrite>* desc) = 0;

    virtual ndk::ScopedAStatus isReconfigurationRequired(
        const ::android::hardware::camera::common::V1_0::helper::CameraMetadata& oldSessionParams,
        const ::android::hardware::camera::common::V1_0::helper::CameraMetadata& newSessionParams,
        bool* reconfigurationRequired) = 0;

    virtual ndk::ScopedAStatus signalStreamFlush(
        const std::vector<int32_t>& streamIds,
        int32_t streamConfigCounter) = 0;

    virtual ndk::ScopedAStatus switchToOffline(
        const std::vector<int32_t>& streamsToKeep,
        CameraOfflineSessionInfo* offlineSessionInfo,
        std::shared_ptr<ICameraOfflineSession>* session) = 0;

    virtual ndk::ScopedAStatus repeatingRequestEnd(
        int32_t frameNumber,
        const std::vector<int32_t>& streamIds) = 0;
};


class BnCameraDeviceSession : public ICameraDeviceSession, public ndk::SharedRefBase {
public:
    ~BnCameraDeviceSession() override = default;
};

}
}
}
}
}
