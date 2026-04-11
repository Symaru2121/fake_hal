#pragma once

#include <android/hardware/camera/provider/2.7/ICameraProvider.h>
#include <android/hardware/camera/provider/2.6/ICameraProviderCallback.h>
#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <android/hardware/camera/common/1.0/types.h>

#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "NoiseOverlay.h"

namespace fake_hal {

using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::camera::common::V1_0::Status;
using ::android::hardware::camera::common::V1_0::VendorTagSection;
using ::android::hardware::camera::provider::V2_4::ICameraProviderCallback;
using ::android::hardware::camera::provider::V2_5::DeviceState;
using ::android::hardware::camera::provider::V2_7::ICameraProvider;
using ::android::hardware::camera::provider::V2_7::CameraIdAndStreamCombination;

class FakeCameraDevice;

class FakeCameraProvider : public ICameraProvider {
public:
    explicit FakeCameraProvider(const std::string& videoFilePath);
    ~FakeCameraProvider() override;

    // --- V2.4 methods ---
    Return<Status> setCallback(
        const ::android::sp<ICameraProviderCallback>& callback) override;

    Return<void> getVendorTags(getVendorTags_cb _hidl_cb) override;

    Return<void> getCameraIdList(getCameraIdList_cb _hidl_cb) override;

    Return<void> isSetTorchModeSupported(
        isSetTorchModeSupported_cb _hidl_cb) override;

    Return<void> getCameraDeviceInterface_V1_x(
        const hidl_string& cameraDeviceName,
        getCameraDeviceInterface_V1_x_cb _hidl_cb) override;

    Return<void> getCameraDeviceInterface_V3_x(
        const hidl_string& cameraDeviceName,
        getCameraDeviceInterface_V3_x_cb _hidl_cb) override;

    // --- V2.5 methods ---
    Return<void> notifyDeviceStateChange(
        ::android::hardware::hidl_bitfield<DeviceState> newState) override;

    // --- V2.6 methods ---
    Return<void> getConcurrentStreamingCameraIds(
        getConcurrentStreamingCameraIds_cb _hidl_cb) override;

    Return<void> isConcurrentStreamCombinationSupported(
        const hidl_vec<
            ::android::hardware::camera::provider::V2_6::CameraIdAndStreamCombination>& configs,
        isConcurrentStreamCombinationSupported_cb _hidl_cb) override;

    // --- V2.7 methods ---
    Return<void> isConcurrentStreamCombinationSupported_2_7(
        const hidl_vec<CameraIdAndStreamCombination>& configs,
        isConcurrentStreamCombinationSupported_2_7_cb _hidl_cb) override;

    static void instantiate(const std::string& videoFilePath);

private:
    std::string videoFilePath_;
    ::android::sp<ICameraProviderCallback> callback_;
    std::mutex callbackMutex_;

    std::map<std::string, ::android::sp<FakeCameraDevice>> devices_;

    static const std::vector<std::string> kCameraIds;

    NoiseOverlay noiseOverlay_;

    static std::string readDeviceSerial();
    static std::string readDeviceProp(const char* propName);
};

} // namespace fake_hal
