/*
 * Lightweight CameraMetadata wrapper for vendor HAL modules.
 * Uses only libcamera_metadata (vendor_available) instead of
 * libcamera_client (not vendor_available).
 * 
 * Provides the same update()/release() API used throughout FakeHAL.
 */
#ifndef FAKE_HAL_CAMERA_METADATA_H
#define FAKE_HAL_CAMERA_METADATA_H

#include <system/camera_metadata.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace android {

class CameraMetadata {
public:
    CameraMetadata()
        : mBuffer(allocate_camera_metadata(128, 4096)) {}

    CameraMetadata(size_t entryCapacity, size_t dataCapacity = 4096)
        : mBuffer(allocate_camera_metadata(entryCapacity, dataCapacity)) {}

    ~CameraMetadata() {
        if (mBuffer) free_camera_metadata(mBuffer);
    }

    // Copy
    CameraMetadata(const CameraMetadata& other) : mBuffer(nullptr) {
        if (other.mBuffer) {
            mBuffer = clone_camera_metadata(other.mBuffer);
        }
    }

    CameraMetadata& operator=(const CameraMetadata& other) {
        if (this != &other) {
            if (mBuffer) free_camera_metadata(mBuffer);
            mBuffer = other.mBuffer ? clone_camera_metadata(other.mBuffer) : nullptr;
        }
        return *this;
    }

    // Move
    CameraMetadata(CameraMetadata&& other) noexcept : mBuffer(other.mBuffer) {
        other.mBuffer = nullptr;
    }

    CameraMetadata& operator=(CameraMetadata&& other) noexcept {
        if (this != &other) {
            if (mBuffer) free_camera_metadata(mBuffer);
            mBuffer = other.mBuffer;
            other.mBuffer = nullptr;
        }
        return *this;
    }

    // Typed update methods matching AOSP CameraMetadata API
    int update(uint32_t tag, const uint8_t* data, size_t count) {
        return updateEntry(tag, data, count, TYPE_BYTE);
    }
    int update(uint32_t tag, const int32_t* data, size_t count) {
        return updateEntry(tag, data, count, TYPE_INT32);
    }
    int update(uint32_t tag, const float* data, size_t count) {
        return updateEntry(tag, data, count, TYPE_FLOAT);
    }
    int update(uint32_t tag, const int64_t* data, size_t count) {
        return updateEntry(tag, data, count, TYPE_INT64);
    }
    int update(uint32_t tag, const double* data, size_t count) {
        return updateEntry(tag, data, count, TYPE_DOUBLE);
    }
    int update(uint32_t tag, const camera_metadata_rational_t* data, size_t count) {
        return updateEntry(tag, data, count, TYPE_RATIONAL);
    }

    // Release ownership of the underlying buffer
    camera_metadata_t* release() {
        camera_metadata_t* ret = mBuffer;
        mBuffer = nullptr;
        return ret;
    }

    // Acquire ownership of a buffer
    void acquire(camera_metadata_t* buf) {
        if (mBuffer) free_camera_metadata(mBuffer);
        mBuffer = buf;
    }

    const camera_metadata_t* getAndLock() const { return mBuffer; }
    void unlock(const camera_metadata_t*) const {}

    bool isEmpty() const { return mBuffer == nullptr || get_camera_metadata_entry_count(mBuffer) == 0; }

private:
    camera_metadata_t* mBuffer;

    // Grow the buffer if needed
    void ensureCapacity(size_t extraEntries, size_t extraData) {
        if (!mBuffer) {
            mBuffer = allocate_camera_metadata(extraEntries + 16, extraData + 512);
            return;
        }

        size_t currentEntries = get_camera_metadata_entry_count(mBuffer);
        size_t maxEntries = get_camera_metadata_entry_capacity(mBuffer);
        size_t currentData = get_camera_metadata_data_count(mBuffer);
        size_t maxData = get_camera_metadata_data_capacity(mBuffer);

        if (currentEntries + extraEntries <= maxEntries &&
            currentData + extraData <= maxData) {
            return; // enough space
        }

        size_t newEntries = std::max(maxEntries * 2, currentEntries + extraEntries + 16);
        size_t newData = std::max(maxData * 2, currentData + extraData + 512);

        camera_metadata_t* newBuf = allocate_camera_metadata(newEntries, newData);
        if (newBuf) {
            append_camera_metadata(newBuf, mBuffer);
            free_camera_metadata(mBuffer);
            mBuffer = newBuf;
        }
    }

    int updateEntry(uint32_t tag, const void* data, size_t count, uint8_t type) {
        (void)type;
        if (!mBuffer) {
            mBuffer = allocate_camera_metadata(32, 2048);
        }

        // Try to find existing entry
        camera_metadata_entry_t entry;
        int res = find_camera_metadata_entry(mBuffer, tag, &entry);
        if (res == 0) {
            // Update existing
            return update_camera_metadata_entry(mBuffer, entry.index, data, count, nullptr);
        }

        // Calculate data size based on tag type
        int tagType = get_camera_metadata_tag_type(tag);
        size_t dataSize = count * camera_metadata_type_size[tagType];

        ensureCapacity(1, dataSize);
        return add_camera_metadata_entry(mBuffer, tag, data, count);
    }
};

} // namespace android

#endif // FAKE_HAL_CAMERA_METADATA_H
