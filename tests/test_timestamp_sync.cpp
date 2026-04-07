#include <gtest/gtest.h>
#include "TimestampSync.h"
#include <thread>
#include <chrono>

using namespace fake_hal;

TEST(TimestampSyncTest, ExposureStartAfterMarkFrameStart) {
    TimestampSync ts;

    struct timespec before;
    clock_gettime(CLOCK_MONOTONIC, &before);
    int64_t beforeNs = (int64_t)before.tv_sec * 1'000'000'000LL + before.tv_nsec;

    ts.markFrameStart();
    int64_t exposureTs = ts.getExposureStartNs();

    EXPECT_GT(exposureTs, beforeNs) << "Exposure start should be after mark";
    EXPECT_LT(exposureTs - beforeNs, 10'000'000LL) << "Shouldn't be more than 10ms after";
}

TEST(TimestampSyncTest, FrameDurationMeasured) {
    TimestampSync ts;

    ts.markFrameStart();
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
    ts.markFrameStart();

    int64_t dur = ts.getActualFrameDurationNs();

    EXPECT_GT(dur, 28'000'000LL);
    EXPECT_LT(dur, 38'000'000LL);
}

TEST(TimestampSyncTest, IMUCorrelationInWindow) {
    TimestampSync ts;
    ts.markFrameStart();

    int64_t exposureStart = ts.getExposureStartNs();
    int64_t exposureTime  = 16'000'000LL;


    int64_t imuTs = exposureStart + exposureTime / 2;
    int64_t corr  = ts.correlateWithIMU(imuTs, exposureTime);

    EXPECT_EQ(corr, imuTs) << "IMU within window should be used as-is";
}

TEST(TimestampSyncTest, IMUCorrelationOutsideWindow) {
    TimestampSync ts;
    ts.markFrameStart();

    int64_t exposureStart = ts.getExposureStartNs();
    int64_t exposureTime  = 16'000'000LL;


    int64_t imuTs = exposureStart - 5'000'000LL;
    int64_t corr  = ts.correlateWithIMU(imuTs, exposureTime);


    EXPECT_EQ(corr, exposureStart + exposureTime / 2);
}

TEST(TimestampSyncTest, DefaultFrameDuration) {
    TimestampSync ts;

    int64_t dur = ts.getActualFrameDurationNs();
    EXPECT_EQ(dur, 33'333'333LL);
}

TEST(TimestampSyncTest, ExposureStartMonotonic) {
    TimestampSync ts;

    ts.markFrameStart();
    int64_t ts1 = ts.getExposureStartNs();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    ts.markFrameStart();
    int64_t ts2 = ts.getExposureStartNs();

    EXPECT_GT(ts2, ts1) << "Exposure timestamps should be monotonically increasing";
}

TEST(TimestampSyncTest, IMUCorrelationAfterWindow) {
    TimestampSync ts;
    ts.markFrameStart();

    int64_t exposureStart = ts.getExposureStartNs();
    int64_t exposureTime  = 16'000'000LL;


    int64_t imuTs = exposureStart + exposureTime + 5'000'000LL;
    int64_t corr  = ts.correlateWithIMU(imuTs, exposureTime);


    EXPECT_EQ(corr, exposureStart + exposureTime / 2);
}

TEST(TimestampSyncTest, FrameDurationClamped) {
    TimestampSync ts;


    ts.markFrameStart();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ts.markFrameStart();

    int64_t dur = ts.getActualFrameDurationNs();
    EXPECT_GE(dur, 16'000'000LL) << "Duration should be clamped to at least 16ms";
}
