#pragma once

#include <memory>
#include <vector>
#include <aidl/android/hardware/camera/device/ndk_types.h>
#include <aidl/android/hardware/camera/device/CaptureResult.h>

namespace aidl {
namespace android {
namespace hardware {
namespace camera {
namespace device {


class ICameraDeviceCallback {
public:
    virtual ~ICameraDeviceCallback() = default;

    virtual ndk::ScopedAStatus processCaptureResult(
        const std::vector<CaptureResult>& results) = 0;

    virtual ndk::ScopedAStatus notify(
        const std::vector<NotifyMsg>& msgs) = 0;
};

}
}
}
}
}
