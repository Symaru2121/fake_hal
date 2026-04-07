#pragma once

#include <string>
#include <aidl/android/hardware/camera/device/ndk_types.h>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace provider {


enum class CameraDeviceStatus : int32_t {
    NOT_PRESENT = 0,
    PRESENT = 1,
    ENUMERATING = 2,
};


enum class TorchModeStatus : int32_t {
    NOT_AVAILABLE = 0,
    AVAILABLE_OFF = 1,
    AVAILABLE_ON = 2,
};

class ICameraProviderCallback {
public:
    virtual ~ICameraProviderCallback() = default;

    virtual ndk::ScopedAStatus cameraDeviceStatusChange(
        const std::string& cameraDeviceName,
        CameraDeviceStatus newStatus) = 0;

    virtual ndk::ScopedAStatus torchModeStatusChange(
        const std::string& cameraDeviceName,
        TorchModeStatus newStatus) = 0;

    virtual ndk::ScopedAStatus physicalCameraDeviceStatusChange(
        const std::string& cameraDeviceName,
        const std::string& physicalCameraDeviceName,
        CameraDeviceStatus newStatus) = 0;
};

}
}
}
}
}
