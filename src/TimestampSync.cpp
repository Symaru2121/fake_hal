#include "TimestampSync.h"
#include <time.h>
#include <algorithm>

namespace fake_hal {

void TimestampSync::markFrameStart() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now = (int64_t)ts.tv_sec * 1'000'000'000LL + ts.tv_nsec;

    int64_t last = lastFrameStartNs_.load();
    if (last > 0) {
        int64_t duration = now - last;

        duration = std::clamp(duration, (int64_t)16'000'000LL, (int64_t)100'000'000LL);
        actualFrameDurationNs_.store(duration);
    }
    lastFrameStartNs_.store(now);


    exposureStartNs_.store(now + 1'000'000LL);
}

int64_t TimestampSync::correlateWithIMU(int64_t imuTs, int64_t exposureTimeNs) const {
    int64_t exposureStart = exposureStartNs_.load();
    int64_t exposureEnd   = exposureStart + exposureTimeNs;


    if (imuTs >= exposureStart && imuTs <= exposureEnd) {
        return imuTs;
    }
    return exposureStart + exposureTimeNs / 2;
}

}
