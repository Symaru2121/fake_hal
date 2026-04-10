#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <aidl/android/hardware/camera/device/CaptureRequest.h>
#include <camera/CameraMetadata.h>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {


// In test builds, alias to the mock android::CameraMetadata so the override
// signatures match the base class virtual declarations.
using CameraMetadata = ::android::CameraMetadata;

struct PhysicalCameraMetadata {
    int64_t fmqMetadataSize = 0;
    std::string physicalCameraId;
    struct { std::vector<uint8_t> metadata; } metadata;
};


struct CaptureResult {
    int32_t frameNumber = 0;
    int64_t fmqResultSize = 0;
    struct { std::vector<uint8_t> metadata; } result;
    std::vector<StreamBuffer> outputBuffers;
    StreamBuffer inputBuffer;
    int32_t partialResult = 0;
    std::vector<PhysicalCameraMetadata> physicalCameraMetadata;
};


enum class MsgType : int32_t {
    ERROR = 1,
    SHUTTER = 2,
};

struct ErrorMsg {
    int32_t frameNumber;
    int32_t errorStreamId;
    int32_t errorCode;
};

struct ShutterMsg {
    int32_t frameNumber;
    int64_t timestamp;
    int32_t readoutTimestamp;
};

struct NotifyMsg {
    MsgType type = MsgType::SHUTTER;
    union {
        ErrorMsg error;
        ShutterMsg shutter;
    } msg;
    NotifyMsg() { std::memset(&msg, 0, sizeof(msg)); }
};

}
}
}
}
}
