#pragma once

#include <system/camera_metadata.h>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>
#include <cassert>

namespace android {


class CameraMetadata {
public:
    CameraMetadata() = default;
    ~CameraMetadata() = default;


    CameraMetadata(const CameraMetadata&) = default;
    CameraMetadata& operator=(const CameraMetadata&) = default;


    CameraMetadata(CameraMetadata&&) = default;
    CameraMetadata& operator=(CameraMetadata&&) = default;


    void update(uint32_t tag, const int32_t* data, size_t count) {
        setEntry(tag, data, count, sizeof(int32_t));
    }


    void update(uint32_t tag, const int64_t* data, size_t count) {
        setEntry(tag, data, count, sizeof(int64_t));
    }


    void update(uint32_t tag, const float* data, size_t count) {
        setEntry(tag, data, count, sizeof(float));
    }


    void update(uint32_t tag, const uint8_t* data, size_t count) {
        setEntry(tag, data, count, sizeof(uint8_t));
    }


    void update(uint32_t tag, const camera_metadata_rational_t* data, size_t count) {
        setEntry(tag, data, count, sizeof(camera_metadata_rational_t));
    }


    struct Entry {
        std::vector<uint8_t> data;
        size_t elemSize = 0;
        size_t count = 0;
    };

    bool exists(uint32_t tag) const {
        return entries_.count(tag) > 0;
    }

    const Entry* find(uint32_t tag) const {
        auto it = entries_.find(tag);
        if (it == entries_.end()) return nullptr;
        return &it->second;
    }


    bool getInt32(uint32_t tag, int32_t* out) const {
        auto* e = find(tag);
        if (!e || e->elemSize != sizeof(int32_t) || e->count == 0) return false;
        memcpy(out, e->data.data(), sizeof(int32_t));
        return true;
    }


    bool getInt64(uint32_t tag, int64_t* out) const {
        auto* e = find(tag);
        if (!e || e->elemSize != sizeof(int64_t) || e->count == 0) return false;
        memcpy(out, e->data.data(), sizeof(int64_t));
        return true;
    }


    bool getFloat(uint32_t tag, float* out) const {
        auto* e = find(tag);
        if (!e || e->elemSize != sizeof(float) || e->count == 0) return false;
        memcpy(out, e->data.data(), sizeof(float));
        return true;
    }


    bool getFloatArray(uint32_t tag, float* out, size_t maxCount) const {
        auto* e = find(tag);
        if (!e || e->elemSize != sizeof(float)) return false;
        size_t n = std::min(e->count, maxCount);
        memcpy(out, e->data.data(), n * sizeof(float));
        return true;
    }


    camera_metadata_t* release() {
        return nullptr;
    }

    size_t entryCount() const { return entries_.size(); }

private:
    std::map<uint32_t, Entry> entries_;

    void setEntry(uint32_t tag, const void* data, size_t count, size_t elemSize) {
        Entry e;
        e.elemSize = elemSize;
        e.count = count;
        e.data.resize(count * elemSize);
        memcpy(e.data.data(), data, count * elemSize);
        entries_[tag] = std::move(e);
    }
};

}


namespace android {
namespace hardware {
namespace camera {
namespace common {
namespace V1_0 {
namespace helper {
    using CameraMetadata = ::android::CameraMetadata;
}
}
}
}
}
}
