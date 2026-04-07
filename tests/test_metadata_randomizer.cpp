#include <gtest/gtest.h>
#include "MetadataRandomizer.h"

using namespace fake_hal;

class MetadataRandomizerTest : public ::testing::Test {
protected:
    MetadataRandomizer rand_;
};


TEST_F(MetadataRandomizerTest, ISOWithinRange) {
    for (int i = 0; i < 500; i++) {
        android::CameraMetadata meta;
        int64_t ts = (int64_t)i * 33'333'333LL;
        rand_.fill(i, &meta, ts);

        int32_t iso = 0;
        ASSERT_TRUE(meta.getInt32(ANDROID_SENSOR_SENSITIVITY, &iso))
            << "ISO tag missing at frame " << i;
        EXPECT_GE(iso, 50)  << "ISO below minimum at frame " << i;
        EXPECT_LE(iso, 3200) << "ISO above maximum at frame " << i;
    }
}


TEST_F(MetadataRandomizerTest, ExposureTimeWithinRange) {
    for (int i = 0; i < 500; i++) {
        android::CameraMetadata meta;
        int64_t ts = (int64_t)i * 33'333'333LL;
        rand_.fill(i, &meta, ts);

        int64_t expNs = 0;
        ASSERT_TRUE(meta.getInt64(ANDROID_SENSOR_EXPOSURE_TIME, &expNs))
            << "Exposure time tag missing at frame " << i;
        EXPECT_GE(expNs, 500'000LL) << "Exposure too short at frame " << i;
        EXPECT_LE(expNs, 33'300'000LL) << "Exposure too long at frame " << i;
    }
}


TEST_F(MetadataRandomizerTest, AWBGainsReasonable) {
    for (int i = 0; i < 500; i++) {
        android::CameraMetadata meta;
        int64_t ts = (int64_t)i * 33'333'333LL;
        rand_.fill(i, &meta, ts);

        float gains[4] = {};
        ASSERT_TRUE(meta.getFloatArray(ANDROID_COLOR_CORRECTION_GAINS, gains, 4))
            << "AWB gains missing at frame " << i;

        for (int ch = 0; ch < 4; ch++) {
            EXPECT_GE(gains[ch], 0.9f) << "AWB gain too low ch=" << ch << " frame=" << i;
            EXPECT_LE(gains[ch], 3.0f) << "AWB gain too high ch=" << ch << " frame=" << i;
        }
    }
}


TEST_F(MetadataRandomizerTest, ValuesActuallyDrift) {
    std::vector<int32_t> isoValues;

    for (int i = 0; i < 100; i++) {
        android::CameraMetadata meta;
        int64_t ts = (int64_t)i * 33'333'333LL;
        rand_.fill(i, &meta, ts);

        int32_t iso = 0;
        meta.getInt32(ANDROID_SENSOR_SENSITIVITY, &iso);
        isoValues.push_back(iso);
    }


    bool allSame = true;
    for (size_t i = 1; i < isoValues.size(); i++) {
        if (isoValues[i] != isoValues[0]) {
            allSame = false;
            break;
        }
    }
    EXPECT_FALSE(allSame) << "ISO values are all identical — randomizer not drifting";
}


TEST_F(MetadataRandomizerTest, TimestampMonotonicallyIncreasing) {
    int64_t prevTs = -1;

    for (int i = 0; i < 100; i++) {
        android::CameraMetadata meta;
        int64_t ts = (int64_t)(i + 1) * 33'333'333LL;
        rand_.fill(i, &meta, ts);

        int64_t readTs = 0;
        ASSERT_TRUE(meta.getInt64(ANDROID_SENSOR_TIMESTAMP, &readTs));
        EXPECT_GT(readTs, prevTs) << "Timestamp not increasing at frame " << i;
        prevTs = readTs;
    }
}


TEST_F(MetadataRandomizerTest, FrameDurationReasonable) {
    for (int i = 0; i < 100; i++) {
        android::CameraMetadata meta;
        int64_t ts = (int64_t)i * 33'333'333LL;
        rand_.fill(i, &meta, ts);

        int64_t duration = 0;
        ASSERT_TRUE(meta.getInt64(ANDROID_SENSOR_FRAME_DURATION, &duration));

        EXPECT_GT(duration, 30'000'000LL);
        EXPECT_LT(duration, 40'000'000LL);
    }
}


TEST_F(MetadataRandomizerTest, RollingShutterSkewPositive) {
    android::CameraMetadata meta;
    rand_.fill(0, &meta, 100'000'000LL);

    int64_t skew = 0;
    ASSERT_TRUE(meta.getInt64(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW, &skew));
    EXPECT_GT(skew, 0);
}
