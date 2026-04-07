#include <gtest/gtest.h>

#include "NoiseOverlay.h"
#include "GyroWarp.h"
#include "JpegEncoder.h"
#include "MetadataRandomizer.h"
#include "RollingShutter.h"
#include "LensShading.h"

#include <jpeglib.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

using namespace fake_hal;

class PipelineIntegrationTest : public ::testing::Test {
protected:
    static constexpr int kWidth  = 128;
    static constexpr int kHeight = 96;
    static constexpr int kYSize  = kWidth * kHeight;
    static constexpr int kNV21Size = kYSize * 3 / 2;


    std::vector<uint8_t> makeSyntheticScene() {
        std::vector<uint8_t> buf(kNV21Size);


        for (int row = 0; row < kHeight; row++) {
            for (int col = 0; col < kWidth; col++) {
                float f = ((float)row / kHeight + (float)col / kWidth) * 0.5f;
                int val = (int)(f * 200.0f + 30.0f);
                buf[row * kWidth + col] = (uint8_t)std::clamp(val, 0, 255);
            }
        }


        uint8_t* uv = buf.data() + kYSize;
        for (int i = 0; i < kYSize / 2; i += 2) {
            uv[i + 0] = 120;
            uv[i + 1] = 140;
        }

        return buf;
    }
};


TEST_F(PipelineIntegrationTest, FullPipelineProducesValidJPEG) {

    auto yuv = makeSyntheticScene();
    auto original = yuv;


    LensShading lensShading(0.4f);
    lensShading.apply(yuv.data(), kWidth, kHeight);


    NoiseOverlay noise(3.0f, 0.35f, 0xCAFEBABE);
    noise.apply(yuv.data(), kWidth, kHeight, 2.0f);


    std::vector<uint8_t> tmp(kNV21Size);
    GyroWarp::shiftNV21(yuv.data(), tmp.data(), kWidth, kHeight, 3, 1);
    std::memcpy(yuv.data(), tmp.data(), kNV21Size);


    RollingShutter rs(33000.0f, 200.0f);
    rs.apply(yuv.data(), kWidth, kHeight, tmp.data(), 0.1f);


    int changed = 0;
    for (int i = 0; i < kYSize; i++) {
        if (yuv[i] != original[i]) changed++;
    }
    EXPECT_GT(changed, kYSize / 4) << "Pipeline should modify most Y pixels";


    JpegEncoder encoder;
    JpegEncoder::ExifData exif;
    exif.iso = 200;
    exif.exposureSec = 1.0f / 60.0f;
    exif.fNumber = 1.85f;
    exif.focalLength = 6.81f;
    exif.imageWidth = kWidth;
    exif.imageHeight = kHeight;

    std::vector<uint8_t> jpeg;
    ASSERT_TRUE(encoder.encode(yuv.data(), kWidth, kHeight, 90, exif, jpeg));


    ASSERT_GE(jpeg.size(), 4u);
    EXPECT_EQ(jpeg[0], 0xFF);
    EXPECT_EQ(jpeg[1], 0xD8);
    EXPECT_EQ(jpeg[jpeg.size() - 2], 0xFF);
    EXPECT_EQ(jpeg[jpeg.size() - 1], 0xD9);
    EXPECT_GT(jpeg.size(), 200u) << "JPEG too small for a real image";


    struct jpeg_decompress_struct dinfo;
    struct jpeg_error_mgr jerr;
    dinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&dinfo);
    jpeg_mem_src(&dinfo, jpeg.data(), jpeg.size());
    ASSERT_EQ(jpeg_read_header(&dinfo, TRUE), JPEG_HEADER_OK);
    EXPECT_EQ((int)dinfo.image_width, kWidth);
    EXPECT_EQ((int)dinfo.image_height, kHeight);
    jpeg_destroy_decompress(&dinfo);
}


TEST_F(PipelineIntegrationTest, MetadataFilledAndRealistic) {
    MetadataRandomizer metaRand;
    android::CameraMetadata meta;

    int64_t ts = 1'000'000'000LL;
    metaRand.fill(0, &meta, ts);


    int32_t iso = 0;
    ASSERT_TRUE(meta.getInt32(ANDROID_SENSOR_SENSITIVITY, &iso));
    EXPECT_GE(iso, 50);
    EXPECT_LE(iso, 3200);


    int64_t expNs = 0;
    ASSERT_TRUE(meta.getInt64(ANDROID_SENSOR_EXPOSURE_TIME, &expNs));
    EXPECT_GT(expNs, 0);


    int64_t readTs = 0;
    ASSERT_TRUE(meta.getInt64(ANDROID_SENSOR_TIMESTAMP, &readTs));
    EXPECT_EQ(readTs, ts);


    float gains[4] = {};
    ASSERT_TRUE(meta.getFloatArray(ANDROID_COLOR_CORRECTION_GAINS, gains, 4));
    for (int i = 0; i < 4; i++) {
        EXPECT_GT(gains[i], 0.5f);
        EXPECT_LT(gains[i], 4.0f);
    }


    EXPECT_TRUE(meta.exists(ANDROID_SENSOR_FRAME_DURATION));
    EXPECT_TRUE(meta.exists(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW));
    EXPECT_TRUE(meta.exists(ANDROID_LENS_APERTURE));
    EXPECT_TRUE(meta.exists(ANDROID_LENS_FOCAL_LENGTH));
}


TEST_F(PipelineIntegrationTest, LensShadingVignettes) {
    LensShading ls(0.5f);

    auto buf = std::vector<uint8_t>(kNV21Size, 200);
    ls.apply(buf.data(), kWidth, kHeight);


    int cx = kWidth / 2, cy = kHeight / 2;
    uint8_t centerVal = buf[cy * kWidth + cx];
    uint8_t cornerVal = buf[0];

    EXPECT_GT(centerVal, cornerVal)
        << "Center (" << (int)centerVal << ") should be brighter than corner (" << (int)cornerVal << ")";


    EXPECT_NEAR(centerVal, 200, 10);

    EXPECT_LT(cornerVal, 190);
}


TEST_F(PipelineIntegrationTest, RollingShutterAppliesPerRowShift) {

    auto buf = std::vector<uint8_t>(kNV21Size, 0);
    int stripeCol = kWidth / 2;
    for (int row = 0; row < kHeight; row++) {
        buf[row * kWidth + stripeCol] = 255;
        if (stripeCol + 1 < kWidth) buf[row * kWidth + stripeCol + 1] = 255;
    }

    std::vector<uint8_t> tmp(kNV21Size);
    RollingShutter rs(33000.0f, 500.0f);
    float gyroRate = 0.5f;
    rs.apply(buf.data(), kWidth, kHeight, tmp.data(), gyroRate);


    auto findStripe = [&](int row) -> int {
        int maxCol = 0;
        uint8_t maxVal = 0;
        for (int col = 0; col < kWidth; col++) {
            if (buf[row * kWidth + col] > maxVal) {
                maxVal = buf[row * kWidth + col];
                maxCol = col;
            }
        }
        return maxCol;
    };

    int topStripe = findStripe(1);
    int botStripe = findStripe(kHeight - 2);


    EXPECT_GE(topStripe, 0);
    EXPECT_LT(topStripe, kWidth);
    EXPECT_GE(botStripe, 0);
    EXPECT_LT(botStripe, kWidth);
}
