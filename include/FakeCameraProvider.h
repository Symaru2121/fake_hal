#pragma once

#include <aidl/android/hardware/camera/provider/BnCameraProvider.h>
#include <aidl/android/hardware/camera/provider/ICameraProviderCallback.h>
#include <aidl/android/hardware/camera/device/ICameraDevice.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "NoiseOverlay.h"

using namespace aidl::android::hardware::camera::provider;
using namespace aidl::android::hardware::camera::device;
using namespace aidl::android::hardware::camera::common;
using aidl::android::hardware::common::fmq::MQDescriptor;

namespace fake_hal {

class FakeCameraDevice;

class FakeCameraProvider : public BnCameraProvider {
public:
    explicit FakeCameraProvider(const std::string& videoFilePath);
    ~FakeCameraProvider() override;


    ndk::ScopedAStatus setCallback(
        const std::shared_ptr<ICameraProviderCallback>& callback) override;

    ndk::ScopedAStatus getVendorTags(
        std::vector<VendorTagSection>* vts) override;

    ndk::ScopedAStatus getCameraIdList(
        std::vector<std::string>* cameraIds) override;

    ndk::ScopedAStatus getCameraDeviceInterface(
        const std::string& cameraDeviceName,
        std::shared_ptr<ICameraDevice>* device) override;

    ndk::ScopedAStatus notifyDeviceStateChange(int64_t deviceState) override;

    ndk::ScopedAStatus getConcurrentCameraIds(
        std::vector<ConcurrentCameraIdCombination>* concurrentCameraIds) override;

    ndk::ScopedAStatus isConcurrentStreamCombinationSupported(
        const std::vector<CameraIdAndStreamCombination>& configs,
        bool* support) override;

    static void instantiate(const std::string& videoFilePath);

private:
    std::string videoFilePath_;
    std::shared_ptr<ICameraProviderCallback> callback_;
    std::mutex callbackMutex_;


    std::map<std::string, std::shared_ptr<FakeCameraDevice>> devices_;

    static const std::vector<std::string> kCameraIds;


    NoiseOverlay noiseOverlay_;


    static std::string readDeviceSerial();
    static std::string readDeviceProp(const char* propName);
};

}
