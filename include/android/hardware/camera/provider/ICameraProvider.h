#ifndef ANDROID_HARDWARE_CAMERA_ICAMERA_PROVIDER_H
#define ANDROID_HARDWARE_CAMERA_ICAMERA_PROVIDER_H

#include <binder/IBinder.h>
#include <string>
#include <vector>

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace V2_4 {

class ICameraProvider : public ::android::RefBase {
public:
    virtual ~ICameraProvider() {}

    virtual ::android::binder::Status getCameraIdList(
        std::vector<std::string>* camera_ids) = 0;

    virtual ::android::binder::Status getCameraDeviceInterface(
        const std::string& camera_id,
        ::android::sp<::android::hardware::camera::device::V3_2::ICameraDevice>* device) = 0;
};

}
}
}
}
}

#endif
