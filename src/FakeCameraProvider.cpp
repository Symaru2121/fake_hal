#include "FakeCameraProvider.h"
#include "FakeCameraDevice.h"

#include <android/hardware/camera/common/1.0/types.h>
#include <hidl/HidlTransportSupport.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/system_properties.h>

#undef LOG_TAG
#define LOG_TAG "FakeHAL_Provider"
#include <log/log.h>

namespace fake_hal {

using ::android::hardware::camera::common::V1_0::Status;

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
        devices_[id] = new FakeCameraDevice(id, videoFilePath_);
    }
    ALOGI("FakeCameraProvider: initialized with video: %s", videoFilePath_.c_str());
}

FakeCameraProvider::~FakeCameraProvider() {
    ALOGI("FakeCameraProvider: destroyed");
}

Return<Status> FakeCameraProvider::setCallback(
    const ::android::sp<ICameraProviderCallback>& callback)
{
    std::lock_guard<std::mutex> lk(callbackMutex_);
    callback_ = callback;
    ALOGI("FakeCameraProvider: setCallback registered");
    return Status::OK;
}

Return<void> FakeCameraProvider::getVendorTags(getVendorTags_cb _hidl_cb) {
    hidl_vec<VendorTagSection> sections;
    _hidl_cb(Status::OK, sections);
    return Void();
}

Return<void> FakeCameraProvider::getCameraIdList(getCameraIdList_cb _hidl_cb) {
    std::vector<hidl_string> deviceNames;
    for (const auto& id : kCameraIds) {
        deviceNames.push_back("device@3.7/fake/" + id);
    }
    hidl_vec<hidl_string> names(deviceNames);
    ALOGI("FakeCameraProvider: getCameraIdList -> %zu devices", deviceNames.size());
    _hidl_cb(Status::OK, names);
    return Void();
}

Return<void> FakeCameraProvider::isSetTorchModeSupported(
    isSetTorchModeSupported_cb _hidl_cb)
{
    _hidl_cb(Status::OK, true);
    return Void();
}

Return<void> FakeCameraProvider::getCameraDeviceInterface_V1_x(
    const hidl_string&,
    getCameraDeviceInterface_V1_x_cb _hidl_cb)
{
    _hidl_cb(Status::OPERATION_NOT_SUPPORTED, nullptr);
    return Void();
}

Return<void> FakeCameraProvider::getCameraDeviceInterface_V3_x(
    const hidl_string& cameraDeviceName,
    getCameraDeviceInterface_V3_x_cb _hidl_cb)
{
    std::string name = cameraDeviceName;
    std::string id = name;
    auto pos = id.rfind('/');
    if (pos != std::string::npos) id = id.substr(pos + 1);

    auto it = devices_.find(id);
    if (it == devices_.end()) {
        ALOGE("FakeCameraProvider: unknown camera id: %s", name.c_str());
        _hidl_cb(Status::ILLEGAL_ARGUMENT, nullptr);
        return Void();
    }

    ALOGI("FakeCameraProvider: getCameraDeviceInterface_V3_x(%s) -> OK", id.c_str());
    _hidl_cb(Status::OK, it->second);
    return Void();
}

Return<void> FakeCameraProvider::notifyDeviceStateChange(
    ::android::hardware::hidl_bitfield<DeviceState> newState)
{
    ALOGI("FakeCameraProvider: notifyDeviceStateChange(0x%lx)", (long)newState);
    return Void();
}

Return<void> FakeCameraProvider::getConcurrentStreamingCameraIds(
    getConcurrentStreamingCameraIds_cb _hidl_cb)
{
    hidl_vec<hidl_vec<hidl_string>> combinations;
    _hidl_cb(Status::OK, combinations);
    return Void();
}

Return<void> FakeCameraProvider::isConcurrentStreamCombinationSupported(
    const hidl_vec<
        ::android::hardware::camera::provider::V2_6::CameraIdAndStreamCombination>&,
    isConcurrentStreamCombinationSupported_cb _hidl_cb)
{
    _hidl_cb(Status::OK, false);
    return Void();
}

Return<void> FakeCameraProvider::isConcurrentStreamCombinationSupported_2_7(
    const hidl_vec<CameraIdAndStreamCombination>&,
    isConcurrentStreamCombinationSupported_2_7_cb _hidl_cb)
{
    _hidl_cb(Status::OK, false);
    return Void();
}

void FakeCameraProvider::instantiate(const std::string& videoFilePath) {
    ::android::hardware::configureRpcThreadpool(8, true);

    ::android::sp<FakeCameraProvider> provider = new FakeCameraProvider(videoFilePath);

    android::status_t status = provider->registerAsService("fake/0");
    if (status != android::OK) {
        ALOGE("FakeCameraProvider: failed to register service: %d", status);
        return;
    }

    ALOGI("FakeCameraProvider: registered as fake/0");
    ::android::hardware::joinRpcThreadpool();
}

} // namespace fake_hal
