#pragma once

#include <cstdint>
#include <vector>
#include <hardware/camera3.h>
#include <aidl/android/hardware/camera/device/ndk_types.h>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {


enum class BufferStatus : int32_t {
    OK = 0,
    ERROR = 1,
};


struct StreamBuffer {
    int32_t streamId = -1;
    int64_t bufferId = 0;
    buffer_handle_t buffer = nullptr;
    BufferStatus status = BufferStatus::OK;
    ndk::ScopedFileDescriptor acquireFence;
    ndk::ScopedFileDescriptor releaseFence;
};


struct BufferCache {
    int32_t streamId = -1;
    int64_t bufferId = 0;
};


struct PhysicalCameraSetting {
    int64_t fmqSettingsSize = 0;
    std::string physicalCameraId;
    struct { std::vector<uint8_t> metadata; } settings;
};


struct CaptureRequest {
    int32_t frameNumber = 0;
    int64_t fmqSettingsSize = 0;
    struct { std::vector<uint8_t> metadata; } settings;
    StreamBuffer inputBuffer;
    std::vector<StreamBuffer> outputBuffers;
    std::vector<PhysicalCameraSetting> physicalCameraSettings;
    int32_t inputWidth = 0;
    int32_t inputHeight = 0;
};


enum class RequestTemplate : int32_t {
    PREVIEW = 1,
    STILL_CAPTURE = 2,
    VIDEO_RECORD = 3,
    VIDEO_SNAPSHOT = 4,
    ZERO_SHUTTER_LAG = 5,
    MANUAL = 6,
};

}
}
}
}
}
