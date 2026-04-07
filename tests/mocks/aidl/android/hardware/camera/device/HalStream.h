#pragma once

#include <cstdint>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {

struct HalStream {
    int32_t id = -1;
    int32_t overrideFormat = 0;
    int64_t producerUsage = 0;
    int64_t consumerUsage = 0;
    int32_t maxBuffers = 0;
    bool    supportOffline = false;
    int32_t overrideDataSpace = 0;
    std::string physicalCameraId;
};

}
}
}
}
}
