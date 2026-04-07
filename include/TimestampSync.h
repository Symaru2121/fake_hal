#pragma once
#include <cstdint>
#include <atomic>

namespace fake_hal {


class TimestampSync {
public:
    TimestampSync() = default;


    void markFrameStart();


    int64_t getExposureStartNs() const { return exposureStartNs_.load(); }


    int64_t correlateWithIMU(int64_t imuTimestampNs, int64_t exposureTimeNs) const;


    int64_t getActualFrameDurationNs() const { return actualFrameDurationNs_.load(); }

private:
    std::atomic<int64_t> exposureStartNs_{0};
    std::atomic<int64_t> lastFrameStartNs_{0};
    std::atomic<int64_t> actualFrameDurationNs_{33'333'333LL};
};

}
