#ifndef ANDROID_HARDWARE_CAMERA_ICAMERA_DEVICE_H
#define ANDROID_HARDWARE_CAMERA_ICAMERA_DEVICE_H

#include <binder/IBinder.h>
#include <string>

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace V3_2 {

class ICameraDevice : public ::android::RefBase {
public:
    virtual ~ICameraDevice() {}

    virtual ::android::binder::Status open(
        const ::android::sp<::android::hardware::camera::device::V3_2::ICameraDeviceCallback>& callback) = 0;

    virtual ::android::binder::Status getCameraInfo(
        ::android::hardware::camera::device::V3_2::CameraInfo* info) = 0;
};

class ICameraDeviceCallback : public ::android::RefBase {
public:
    virtual ~ICameraDeviceCallback() {}
};

struct CameraInfo {
    std::string cameraId;
    int facing;
    int orientation;
};

}
}
}
}
}

#endif
