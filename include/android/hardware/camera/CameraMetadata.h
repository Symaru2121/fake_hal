#ifndef ANDROID_HARDWARE_CAMERA_METADATA_H
#define ANDROID_HARDWARE_CAMERA_METADATA_H

#include <stdint.h>
#include <vector>
#include <string>

namespace android {
namespace hardware {
namespace camera {
namespace metadata {

class CameraMetadata {
public:
    CameraMetadata() {}
    ~CameraMetadata() {}

    template<typename T>
    int update(uint32_t tag, const T& value) {

        return 0;
    }

    template<typename T>
    int get(uint32_t tag, T* value) const {

        return 0;
    }

    size_t size() const { return 0; }
    const uint8_t* data() const { return nullptr; }
};

}
}
}
}

#endif
