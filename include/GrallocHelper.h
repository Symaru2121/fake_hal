#pragma once


#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <mutex>


#include <cutils/native_handle.h>


// In AOSP builds, GRALLOC_USAGE_SW_WRITE_OFTEN is an enum in hardware/gralloc.h.
// Only define it as a macro for test builds where the real header isn't available.
#ifdef FAKE_HAL_TEST_BUILD
#ifndef GRALLOC_USAGE_SW_WRITE_OFTEN
#define GRALLOC_USAGE_SW_WRITE_OFTEN 0x00000030
#endif
#endif

namespace fake_hal {

enum class GrallocVersion {
    GRALLOC4,
    GRALLOC3,
    MMAP_FALLBACK
};

class GrallocHelper {
public:

    static GrallocHelper& getInstance();


    GrallocVersion version() const { return version_; }


    const char* versionString() const;


    bool lock(buffer_handle_t handle, int width, int height,
              uint32_t usage, void** outPtr);


    int unlock(buffer_handle_t handle);


    GrallocHelper(const GrallocHelper&) = delete;
    GrallocHelper& operator=(const GrallocHelper&) = delete;

private:
    GrallocHelper();

    GrallocVersion version_;


    void* mapperService_ = nullptr;


    bool lockGralloc4(buffer_handle_t h, int w, int h2, uint32_t usage, void** ptr);
    bool lockGralloc3(buffer_handle_t h, int w, int h2, uint32_t usage, void** ptr);
    bool lockMmap(buffer_handle_t h, int w, int h2, uint32_t usage, void** ptr);


    int unlockGralloc4(buffer_handle_t h);
    int unlockGralloc3(buffer_handle_t h);
    int unlockMmap(buffer_handle_t h);


    struct MappedBuffer {
        void*  ptr;
        size_t size;
        int    fd;
    };
    std::unordered_map<const native_handle_t*, MappedBuffer> mappings_;
    std::mutex mappingsMutex_;
};

}
