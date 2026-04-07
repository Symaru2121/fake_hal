#pragma once

#include <memory>
#include <string>
#include <vector>
#include <aidl/android/hardware/camera/device/ndk_types.h>
#include <aidl/android/hardware/camera/device/BnCameraDevice.h>
#include <aidl/android/hardware/camera/provider/ICameraProviderCallback.h>
#include <aidl/android/hardware/camera/common/Status.h>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace provider {


struct VendorTagSection {
    std::string sectionName;
    struct VendorTag {
        uint32_t tagId;
        std::string tagName;
        int32_t tagType;
    };
    std::vector<VendorTag> tags;
};


struct ConcurrentCameraIdCombination {
    std::vector<std::string> combination;
};


struct CameraIdAndStreamCombination {
    std::string cameraId;
    device::StreamConfiguration streamConfiguration;
};


class ICameraProvider {
public:
    virtual ~ICameraProvider() = default;


    static constexpr const char* descriptor =
        "android.hardware.camera.provider.ICameraProvider";

    virtual ndk::ScopedAStatus setCallback(
        const std::shared_ptr<ICameraProviderCallback>& callback) = 0;

    virtual ndk::ScopedAStatus getVendorTags(
        std::vector<VendorTagSection>* vts) = 0;

    virtual ndk::ScopedAStatus getCameraIdList(
        std::vector<std::string>* cameraIds) = 0;

    virtual ndk::ScopedAStatus getCameraDeviceInterface(
        const std::string& cameraDeviceName,
        std::shared_ptr<device::ICameraDevice>* device) = 0;

    virtual ndk::ScopedAStatus notifyDeviceStateChange(int64_t deviceState) = 0;

    virtual ndk::ScopedAStatus getConcurrentCameraIds(
        std::vector<ConcurrentCameraIdCombination>* concurrentCameraIds) = 0;

    virtual ndk::ScopedAStatus isConcurrentStreamCombinationSupported(
        const std::vector<CameraIdAndStreamCombination>& configs,
        bool* support) = 0;
};


class BnCameraProvider : public ICameraProvider, public ndk::SharedRefBase {
public:
    ~BnCameraProvider() override = default;
};

}
}
}
}
}
