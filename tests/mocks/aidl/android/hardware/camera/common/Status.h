#pragma once

#include <cstdint>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace common {

enum class Status : int32_t {
    OK = 0,
    ILLEGAL_ARGUMENT = 1,
    CAMERA_IN_USE = 2,
    MAX_CAMERAS_IN_USE = 3,
    METHOD_NOT_SUPPORTED = 4,
    OPERATION_NOT_SUPPORTED = 5,
    CAMERA_DISCONNECTED = 6,
    INTERNAL_ERROR = 7,
};

}
}
}
}
}
