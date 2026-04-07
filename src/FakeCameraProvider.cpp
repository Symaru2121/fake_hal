#include "FakeCameraProvider.h"
#include "FakeCameraDevice.h"

#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/system_properties.h>

#define LOG_TAG "FakeHAL_Provider"
#include <log/log.h>

namespace fake_hal {

const std::vector<std::string> FakeCameraProvider::kCameraIds = {"0", "1"};

std::string FakeCameraProvider::readDeviceSerial() {

    char buf[64] = {};
    FILE* f = fopen("/sys/devices/soc0/serial_number", "r");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            fclose(f);

            size_t len = strlen(buf);
            while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
                buf[--len] = '\0';
            if (len > 0) return buf;
        } else {
            fclose(f);
        }
    }

#ifdef __ANDROID__

    char prop[92] = {};
    if (__system_property_get("ro.serialno", prop) > 0) return prop;
#endif


    std::ifstream idFile("/data/misc/fake_hal_id");
    if (idFile.good()) {
        std::string id;
        std::getline(idFile, id);
        if (!id.empty()) return id;
    }

    return "UNKNOWN_SERIAL";
}

std::string FakeCameraProvider::readDeviceProp(const char* propName) {
#ifdef __ANDROID__
    char prop[92] = {};
    if (__system_property_get(propName, prop) > 0) return prop;
#endif
    (void)propName;
    return "Pixel 7";
}

FakeCameraProvider::FakeCameraProvider(const std::string& videoFilePath)
    : videoFilePath_(videoFilePath)
    , noiseOverlay_(3.0f, 0.35f)
{

    std::string serial = readDeviceSerial();
    std::string model  = readDeviceProp("ro.product.model");
    noiseOverlay_.setSensorFingerprint(serial, model);
    ALOGI("FakeCameraProvider: FPN fingerprint set for device %s",
          serial.substr(0, 4).c_str());


    for (const auto& id : kCameraIds) {
        devices_[id] = std::make_shared<FakeCameraDevice>(id, videoFilePath_);
    }
    ALOGI("FakeCameraProvider: initialized with video: %s", videoFilePath_.c_str());
}

FakeCameraProvider::~FakeCameraProvider() {
    ALOGI("FakeCameraProvider: destroyed");
}

ndk::ScopedAStatus FakeCameraProvider::setCallback(
    const std::shared_ptr<ICameraProviderCallback>& callback)
{
    std::lock_guard<std::mutex> lk(callbackMutex_);
    callback_ = callback;
    ALOGI("FakeCameraProvider: setCallback registered");
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraProvider::getVendorTags(
    std::vector<VendorTagSection>* vts)
{
    vts->clear();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraProvider::getCameraIdList(
    std::vector<std::string>* cameraIds)
{
    *cameraIds = kCameraIds;
    ALOGI("FakeCameraProvider: getCameraIdList -> [%s]",
          kCameraIds.size() == 2 ? "0, 1" : "0");
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraProvider::getCameraDeviceInterface(
    const std::string& cameraDeviceName,
    std::shared_ptr<ICameraDevice>* device)
{

    std::string id = cameraDeviceName;

    auto pos = id.rfind('/');
    if (pos != std::string::npos) id = id.substr(pos + 1);

    auto it = devices_.find(id);
    if (it == devices_.end()) {
        ALOGE("FakeCameraProvider: unknown camera id: %s", cameraDeviceName.c_str());
        return ndk::ScopedAStatus::fromServiceSpecificError(
            static_cast<int32_t>(Status::ILLEGAL_ARGUMENT));
    }

    *device = it->second;
    ALOGI("FakeCameraProvider: getCameraDeviceInterface(%s) -> OK", id.c_str());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraProvider::notifyDeviceStateChange(int64_t deviceState) {
    ALOGI("FakeCameraProvider: notifyDeviceStateChange(0x%lx)", (long)deviceState);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraProvider::getConcurrentCameraIds(
    std::vector<ConcurrentCameraIdCombination>* concurrentCameraIds)
{
    concurrentCameraIds->clear();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FakeCameraProvider::isConcurrentStreamCombinationSupported(
    const std::vector<CameraIdAndStreamCombination>&,
    bool* support)
{
    *support = false;
    return ndk::ScopedAStatus::ok();
}


void FakeCameraProvider::instantiate(const std::string& videoFilePath) {
    ABinderProcess_setThreadPoolMaxThreadCount(4);

    auto provider = ndk::SharedRefBase::make<FakeCameraProvider>(videoFilePath);

    const std::string serviceName =
        std::string(ICameraProvider::descriptor) + "/fake/0";

    binder_status_t status = AServiceManager_addService(
        provider->asBinder().get(),
        serviceName.c_str()
    );

    if (status != STATUS_OK) {
        ALOGE("FakeCameraProvider: failed to register service '%s': %d",
              serviceName.c_str(), status);
        return;
    }

    ALOGI("FakeCameraProvider: registered as '%s'", serviceName.c_str());
    ABinderProcess_joinThreadPool();
}

}
