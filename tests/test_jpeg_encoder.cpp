#include <gtest/gtest.h>
#include "JpegEncoder.h"

#include <jpeglib.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

using namespace fake_hal;

class JpegEncoderTest : public ::testing::Test {
protected:
    static constexpr int kWidth  = 64;
    static constexpr int kHeight = 48;
    static constexpr int kYSize  = kWidth * kHeight;
    static constexpr int kNV21Size = kYSize * 3 / 2;

    JpegEncoder encoder_;


    std::vector<uint8_t> makeTestImage() {
        std::vector<uint8_t> buf(kNV21Size);

        for (int row = 0; row < kHeight; row++) {
            for (int col = 0; col < kWidth; col++) {
                buf[row * kWidth + col] = (uint8_t)(col * 255 / (kWidth - 1));
            }
        }

        std::memset(buf.data() + kYSize, 128, kNV21Size - kYSize);
        return buf;
    }

    JpegEncoder::ExifData makeExif(int iso = 200) {
        JpegEncoder::ExifData exif;
        exif.iso = iso;
        exif.exposureSec = 1.0f / 60.0f;
        exif.fNumber = 1.85f;
        exif.focalLength = 6.81f;
        exif.imageWidth = kWidth;
        exif.imageHeight = kHeight;
        return exif;
    }
};


TEST_F(JpegEncoderTest, StartsWithSOI) {
    auto nv21 = makeTestImage();
    auto exif = makeExif();
    std::vector<uint8_t> jpeg;

    ASSERT_TRUE(encoder_.encode(nv21.data(), kWidth, kHeight, 90, exif, jpeg));
    ASSERT_GE(jpeg.size(), 2u);
    EXPECT_EQ(jpeg[0], 0xFF);
    EXPECT_EQ(jpeg[1], 0xD8);
}


TEST_F(JpegEncoderTest, ContainsAPP1Exif) {
    auto nv21 = makeTestImage();
    auto exif = makeExif();
    std::vector<uint8_t> jpeg;

    ASSERT_TRUE(encoder_.encode(nv21.data(), kWidth, kHeight, 90, exif, jpeg));


    ASSERT_GE(jpeg.size(), 4u);
    EXPECT_EQ(jpeg[2], 0xFF);
    EXPECT_EQ(jpeg[3], 0xE1);


    ASSERT_GE(jpeg.size(), 10u);
    EXPECT_EQ(jpeg[6], 'E');
    EXPECT_EQ(jpeg[7], 'x');
    EXPECT_EQ(jpeg[8], 'i');
    EXPECT_EQ(jpeg[9], 'f');
}


TEST_F(JpegEncoderTest, ExifContainsCorrectISO) {
    auto nv21 = makeTestImage();
    int testISO = 400;
    auto exif = makeExif(testISO);
    std::vector<uint8_t> jpeg;

    ASSERT_TRUE(encoder_.encode(nv21.data(), kWidth, kHeight, 90, exif, jpeg));


    bool found = false;
    for (size_t i = 0; i + 12 < jpeg.size(); i++) {
        if (jpeg[i] == 0x27 && jpeg[i + 1] == 0x88 &&
            jpeg[i + 2] == 0x03 && jpeg[i + 3] == 0x00) {

            uint32_t val = jpeg[i + 8] | (jpeg[i + 9] << 8) |
                           (jpeg[i + 10] << 16) | (jpeg[i + 11] << 24);
            EXPECT_EQ((int)val, testISO) << "ISO mismatch in EXIF";
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "ISO tag 0x8827 not found in EXIF";
}


TEST_F(JpegEncoderTest, DecodesBackCorrectly) {
    auto nv21 = makeTestImage();
    auto exif = makeExif();
    std::vector<uint8_t> jpeg;

    ASSERT_TRUE(encoder_.encode(nv21.data(), kWidth, kHeight, 95, exif, jpeg));


    struct jpeg_decompress_struct dinfo;
    struct jpeg_error_mgr jerr;
    dinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&dinfo);

    jpeg_mem_src(&dinfo, jpeg.data(), jpeg.size());
    ASSERT_EQ(jpeg_read_header(&dinfo, TRUE), JPEG_HEADER_OK);

    dinfo.out_color_space = JCS_YCbCr;
    jpeg_start_decompress(&dinfo);

    EXPECT_EQ((int)dinfo.output_width, kWidth);
    EXPECT_EQ((int)dinfo.output_height, kHeight);
    EXPECT_EQ(dinfo.output_components, 3);


    std::vector<uint8_t> decoded(kWidth * kHeight * 3);
    while (dinfo.output_scanline < dinfo.output_height) {
        JSAMPROW row = decoded.data() + dinfo.output_scanline * kWidth * 3;
        jpeg_read_scanlines(&dinfo, &row, 1);
    }

    jpeg_finish_decompress(&dinfo);
    jpeg_destroy_decompress(&dinfo);


    int cx = kWidth / 2, cy = kHeight / 2;
    int idx = (cy * kWidth + cx) * 3;
    uint8_t Y = decoded[idx];

    EXPECT_NEAR(Y, 128, 20) << "Decoded Y at center doesn't match expected";
}


TEST_F(JpegEncoderTest, SizeReasonable) {
    auto nv21 = makeTestImage();
    auto exif = makeExif();
    std::vector<uint8_t> jpeg;

    ASSERT_TRUE(encoder_.encode(nv21.data(), kWidth, kHeight, 90, exif, jpeg));

    EXPECT_GT(jpeg.size(), 100u) << "JPEG too small";
    EXPECT_LT(jpeg.size(), (size_t)kNV21Size)
        << "JPEG larger than raw NV21 — compression not working";
}


TEST_F(JpegEncoderTest, QualityAffectsSize) {
    auto nv21 = makeTestImage();
    auto exif = makeExif();

    std::vector<uint8_t> jpegLow, jpegHigh;
    ASSERT_TRUE(encoder_.encode(nv21.data(), kWidth, kHeight, 30, exif, jpegLow));
    ASSERT_TRUE(encoder_.encode(nv21.data(), kWidth, kHeight, 95, exif, jpegHigh));

    EXPECT_LT(jpegLow.size(), jpegHigh.size())
        << "Lower quality should produce smaller file";
}


TEST_F(JpegEncoderTest, EndsWithEOI) {
    auto nv21 = makeTestImage();
    auto exif = makeExif();
    std::vector<uint8_t> jpeg;

    ASSERT_TRUE(encoder_.encode(nv21.data(), kWidth, kHeight, 90, exif, jpeg));

    ASSERT_GE(jpeg.size(), 2u);
    EXPECT_EQ(jpeg[jpeg.size() - 2], 0xFF);
    EXPECT_EQ(jpeg[jpeg.size() - 1], 0xD9);
}
