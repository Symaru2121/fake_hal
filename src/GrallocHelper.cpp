

#define LOG_TAG "FakeHAL_Gralloc"
#include <log/log.h>

#include "GrallocHelper.h"

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>


#if __has_include(<android/hardware/graphics/mapper/4.0/IMapper.h>)
#define HAS_GRALLOC4 1
#include <android/hardware/graphics/mapper/4.0/IMapper.h>
using IMapper4   = android::hardware::graphics::mapper::V4_0::IMapper;
using Error4     = android::hardware::graphics::mapper::V4_0::Error;
#else
#define HAS_GRALLOC4 0
#endif

#if __has_include(<android/hardware/graphics/mapper/3.0/IMapper.h>)
#define HAS_GRALLOC3 1
#include <android/hardware/graphics/mapper/3.0/IMapper.h>
using IMapper3   = android::hardware::graphics::mapper::V3_0::IMapper;
using Error3     = android::hardware::graphics::mapper::V3_0::Error;
#else
#define HAS_GRALLOC3 0
#endif

namespace fake_hal {


GrallocHelper& GrallocHelper::getInstance() {
    static GrallocHelper instance;
    return instance;
}


GrallocHelper::GrallocHelper() {

#if HAS_GRALLOC4
    {
        auto svc = IMapper4::getService();
        if (svc != nullptr) {
            mapperService_ = svc.get();


            svc->incStrong(this);
            version_ = GrallocVersion::GRALLOC4;
            ALOGI("GrallocHelper: using gralloc4 (IMapper 4.0)");
            return;
        }
    }
#endif


#if HAS_GRALLOC3
    {
        auto svc = IMapper3::getService();
        if (svc != nullptr) {
            mapperService_ = svc.get();
            svc->incStrong(this);
            version_ = GrallocVersion::GRALLOC3;
            ALOGI("GrallocHelper: using gralloc3 (IMapper 3.0)");
            return;
        }
    }
#endif


    version_ = GrallocVersion::MMAP_FALLBACK;
    ALOGW("GrallocHelper: falling back to direct mmap (no IMapper service found)");
}

const char* GrallocHelper::versionString() const {
    switch (version_) {
        case GrallocVersion::GRALLOC4:      return "gralloc4 (IMapper 4.0)";
        case GrallocVersion::GRALLOC3:      return "gralloc3 (IMapper 3.0)";
        case GrallocVersion::MMAP_FALLBACK: return "mmap fallback";
    }
    return "unknown";
}


bool GrallocHelper::lock(buffer_handle_t handle, int width, int height,
                         uint32_t usage, void** outPtr) {
    if (!handle || !outPtr) {
        ALOGE("GrallocHelper::lock: null handle or outPtr");
        return false;
    }
    *outPtr = nullptr;

    switch (version_) {
        case GrallocVersion::GRALLOC4:
            return lockGralloc4(handle, width, height, usage, outPtr);
        case GrallocVersion::GRALLOC3:
            return lockGralloc3(handle, width, height, usage, outPtr);
        case GrallocVersion::MMAP_FALLBACK:
            return lockMmap(handle, width, height, usage, outPtr);
    }
    return false;
}


int GrallocHelper::unlock(buffer_handle_t handle) {
    if (!handle) return -1;

    switch (version_) {
        case GrallocVersion::GRALLOC4:
            return unlockGralloc4(handle);
        case GrallocVersion::GRALLOC3:
            return unlockGralloc3(handle);
        case GrallocVersion::MMAP_FALLBACK:
            return unlockMmap(handle);
    }
    return -1;
}


bool GrallocHelper::lockGralloc4(buffer_handle_t h, int w, int h2,
                                  uint32_t usage, void** ptr) {
#if HAS_GRALLOC4
    auto mapper = static_cast<IMapper4*>(mapperService_);
    if (!mapper) {
        ALOGE("lockGralloc4: mapper service is null");
        return false;
    }

    IMapper4::Rect accessRegion = {0, 0, w, h2};


    bool success = false;
    auto ret = mapper->lock(
        const_cast<native_handle_t*>(h),
        static_cast<uint64_t>(usage),
        accessRegion,
        android::hardware::hidl_handle(),
        [&](Error4 err, void* mappedPtr) {
            if (err == Error4::NONE && mappedPtr != nullptr) {
                *ptr = mappedPtr;
                success = true;
            } else {
                ALOGE("lockGralloc4: IMapper lock returned error %d", (int)err);
            }
        });

    if (!ret.isOk()) {
        ALOGE("lockGralloc4: HIDL call failed: %s", ret.description().c_str());
        return false;
    }

    return success;
#else
    (void)h; (void)w; (void)h2; (void)usage; (void)ptr;
    ALOGE("lockGralloc4: compiled without gralloc4 headers");
    return false;
#endif
}

int GrallocHelper::unlockGralloc4(buffer_handle_t h) {
#if HAS_GRALLOC4
    auto mapper = static_cast<IMapper4*>(mapperService_);
    if (!mapper) return -1;

    int fenceFd = -1;
    auto ret = mapper->unlock(
        const_cast<native_handle_t*>(h),
        [&](Error4 err, const android::hardware::hidl_handle& fence) {
            if (err != Error4::NONE) {
                ALOGE("unlockGralloc4: IMapper unlock error %d", (int)err);
            }

            if (fence.getNativeHandle() &&
                fence.getNativeHandle()->numFds > 0) {
                fenceFd = dup(fence.getNativeHandle()->data[0]);
            }
        });

    if (!ret.isOk()) {
        ALOGE("unlockGralloc4: HIDL call failed: %s", ret.description().c_str());
    }
    return fenceFd;
#else
    (void)h;
    return -1;
#endif
}


bool GrallocHelper::lockGralloc3(buffer_handle_t h, int w, int h2,
                                  uint32_t usage, void** ptr) {
#if HAS_GRALLOC3
    auto mapper = static_cast<IMapper3*>(mapperService_);
    if (!mapper) {
        ALOGE("lockGralloc3: mapper service is null");
        return false;
    }

    IMapper3::Rect accessRegion = {0, 0, w, h2};

    bool success = false;
    auto ret = mapper->lock(
        const_cast<native_handle_t*>(h),
        static_cast<uint64_t>(usage),
        accessRegion,
        android::hardware::hidl_handle(),
        [&](Error3 err, void* mappedPtr, int32_t /*bytesPerPixel*/, int32_t /*bytesPerStride*/) {
            if (err == Error3::NONE && mappedPtr != nullptr) {
                *ptr = mappedPtr;
                success = true;
            } else {
                ALOGE("lockGralloc3: IMapper lock returned error %d", (int)err);
            }
        });

    if (!ret.isOk()) {
        ALOGE("lockGralloc3: HIDL call failed: %s", ret.description().c_str());
        return false;
    }

    return success;
#else
    (void)h; (void)w; (void)h2; (void)usage; (void)ptr;
    ALOGE("lockGralloc3: compiled without gralloc3 headers");
    return false;
#endif
}

int GrallocHelper::unlockGralloc3(buffer_handle_t h) {
#if HAS_GRALLOC3
    auto mapper = static_cast<IMapper3*>(mapperService_);
    if (!mapper) return -1;

    int fenceFd = -1;
    auto ret = mapper->unlock(
        const_cast<native_handle_t*>(h),
        [&](Error3 err, const android::hardware::hidl_handle& fence) {
            if (err != Error3::NONE) {
                ALOGE("unlockGralloc3: IMapper unlock error %d", (int)err);
            }
            if (fence.getNativeHandle() &&
                fence.getNativeHandle()->numFds > 0) {
                fenceFd = dup(fence.getNativeHandle()->data[0]);
            }
        });

    if (!ret.isOk()) {
        ALOGE("unlockGralloc3: HIDL call failed: %s", ret.description().c_str());
    }
    return fenceFd;
#else
    (void)h;
    return -1;
#endif
}


bool GrallocHelper::lockMmap(buffer_handle_t h, int w, int h2,
                              uint32_t , void** ptr) {
    const native_handle_t* nh = static_cast<const native_handle_t*>(h);
    if (!nh || nh->numFds < 1) {
        ALOGE("lockMmap: invalid native_handle (numFds=%d)",
              nh ? nh->numFds : -1);
        return false;
    }

    int fd = nh->data[0];
    if (fd < 0) {
        ALOGE("lockMmap: invalid fd %d in native_handle", fd);
        return false;
    }


    size_t size;
    if (h2 <= 1) {

        size = (size_t)w;
    } else {

        size = (size_t)(w * h2 * 3 / 2);
    }

    void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        ALOGE("lockMmap: mmap failed for fd=%d size=%zu: %s",
              fd, size, strerror(errno));
        return false;
    }

    *ptr = mapped;


    {
        std::lock_guard<std::mutex> lk(mappingsMutex_);
        mappings_[nh] = {mapped, size, fd};
    }

    ALOGV("lockMmap: mapped fd=%d -> %p (%zu bytes)", fd, mapped, size);
    return true;
}

int GrallocHelper::unlockMmap(buffer_handle_t h) {
    const native_handle_t* nh = static_cast<const native_handle_t*>(h);

    MappedBuffer mb = {};
    {
        std::lock_guard<std::mutex> lk(mappingsMutex_);
        auto it = mappings_.find(nh);
        if (it == mappings_.end()) {
            ALOGE("unlockMmap: handle %p not found in mappings", h);
            return -1;
        }
        mb = it->second;
        mappings_.erase(it);
    }


    if (msync(mb.ptr, mb.size, MS_SYNC) != 0) {
        ALOGW("unlockMmap: msync failed: %s", strerror(errno));
    }

    if (munmap(mb.ptr, mb.size) != 0) {
        ALOGE("unlockMmap: munmap failed: %s", strerror(errno));
    }

    ALOGV("unlockMmap: unmapped %p (%zu bytes)", mb.ptr, mb.size);
    return -1;
}

}
