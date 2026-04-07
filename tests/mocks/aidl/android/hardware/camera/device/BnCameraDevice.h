#pragma once

#include <memory>
#include <string>
#include <aidl/android/hardware/camera/device/ndk_types.h>
#include <aidl/android/hardware/camera/device/BnCameraDeviceSession.h>
#include <aidl/android/hardware/camera/device/ICameraDeviceCallback.h>
#include <aidl/android/hardware/camera/device/StreamConfiguration.h>
#include <aidl/android/hardware/camera/common/Status.h>
#include <camera/CameraMetadata.h>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {


class ICameraInjectionSession {
public:
    virtual ~ICameraInjectionSession() = default;
};


struct CameraResourceCost {
    int32_t resourceCost = 100;
    std::vector<std::string> conflictingDevices;
};


class ICameraDevice {
public:
    virtual ~ICameraDevice() = default;

    virtual ndk::ScopedAStatus getCameraCharacteristics(
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata* chars) = 0;

    virtual ndk::ScopedAStatus getPhysicalCameraCharacteristics(
        const std::string& physicalCameraId,
        ::android::hardware::camera::common::V1_0::helper::CameraMetadata* chars) = 0;

    virtual ndk::ScopedAStatus getResourceCost(CameraResourceCost* cost) = 0;

    virtual ndk::ScopedAStatus isStreamCombinationSupported(
        const StreamConfiguration& config, bool* support) = 0;

    virtual ndk::ScopedAStatus open(
        const std::shared_ptr<ICameraDeviceCallback>& callback,
        std::shared_ptr<ICameraDeviceSession>* session) = 0;

    virtual ndk::ScopedAStatus openInjectionSession(
        const std::shared_ptr<ICameraDeviceCallback>& callback,
        std::shared_ptr<ICameraInjectionSession>* session) = 0;

    virtual ndk::ScopedAStatus setTorchMode(bool on) = 0;
    virtual ndk::ScopedAStatus turnOnTorchWithStrengthLevel(int32_t strength) = 0;
    virtual ndk::ScopedAStatus getTorchStrengthLevel(int32_t* strength) = 0;
};


class BnCameraDevice : public ICameraDevice, public ndk::SharedRefBase {
public:
    ~BnCameraDevice() override = default;
};

}
}
}
}
}
