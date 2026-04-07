#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <hardware/camera3.h>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {


enum class StreamRotation : int32_t {
    ROTATION_0 = 0,
    ROTATION_90 = 1,
    ROTATION_180 = 2,
    ROTATION_270 = 3,
};


enum class StreamType : int32_t {
    OUTPUT = 0,
    INPUT = 1,
};


struct Stream {
    int32_t id = -1;
    StreamType streamType = StreamType::OUTPUT;
    int32_t width = 0;
    int32_t height = 0;
    int32_t format = 0;
    int64_t usage = 0;
    int32_t dataSpace = 0;
    StreamRotation rotation = StreamRotation::ROTATION_0;
    std::string physicalCameraId;
    int32_t bufferSize = 0;
    int32_t groupId = -1;

    std::vector<int32_t> sensorPixelModesUsed;
    int32_t dynamicRangeProfile = 0;
    int32_t useCase = 0;
    int32_t colorSpace = 0;
};


struct StreamConfiguration {
    std::vector<Stream> streams;
    int32_t operationMode = 0;

    struct SessionParams {
        std::vector<uint8_t> metadata;
    } sessionParams;
    int32_t streamConfigCounter = 0;
    bool multiResolutionInputImage = false;
};


enum class StreamConfigurationMode : int32_t {
    NORMAL_MODE = 0,
    CONSTRAINED_HIGH_SPEED_MODE = 1,
};

}
}
}
}
}
