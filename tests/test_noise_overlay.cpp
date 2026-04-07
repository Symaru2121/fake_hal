#include <gtest/gtest.h>
#include "NoiseOverlay.h"

#include <cmath>
#include <cstring>
#include <numeric>
#include <vector>
#include <algorithm>
#include <string>

using namespace fake_hal;

class NoiseOverlayTest : public ::testing::Test {
protected:
    static constexpr int kWidth  = 64;
    static constexpr int kHeight = 48;
    static constexpr int kYSize  = kWidth * kHeight;
    static constexpr int kNV21Size = kYSize * 3 / 2;

    std::vector<uint8_t> makeUniform(uint8_t val) {
        std::vector<uint8_t> buf(kNV21Size, 128);
        std::memset(buf.data(), val, kYSize);
        return buf;
    }


    double stddev(const uint8_t* a, const uint8_t* b, int count) {
        double sum = 0, sum2 = 0;
        for (int i = 0; i < count; i++) {
            double diff = (double)a[i] - (double)b[i];
            sum += diff;
            sum2 += diff * diff;
        }
        double mean = sum / count;
        double var = sum2 / count - mean * mean;
        return std::sqrt(std::max(var, 0.0));
    }
};


TEST_F(NoiseOverlayTest, PixelsActuallyChange) {
    NoiseOverlay noise(3.0f, 0.35f, 0x12345);
    auto original = makeUniform(128);
    auto noisy = original;

    noise.apply(noisy.data(), kWidth, kHeight, 1.0f);

    int changed = 0;
    for (int i = 0; i < kYSize; i++) {
        if (noisy[i] != original[i]) changed++;
    }


    EXPECT_GT(changed, kYSize / 2)
        << "Too few pixels changed: " << changed << "/" << kYSize;
}


TEST_F(NoiseOverlayTest, DarkPixelsHaveMoreNoise) {
    NoiseOverlay noiseDark(3.0f, 0.35f, 0xABCD);
    NoiseOverlay noiseBright(3.0f, 0.35f, 0xABCD);


    auto dark = makeUniform(30);
    auto darkOrig = dark;
    noiseDark.applyLuma(dark.data(), kWidth, kHeight, 4.0f);
    double darkStd = stddev(dark.data(), darkOrig.data(), kYSize);


    auto bright = makeUniform(200);
    auto brightOrig = bright;
    noiseBright.applyLuma(bright.data(), kWidth, kHeight, 4.0f);
    double brightStd = stddev(bright.data(), brightOrig.data(), kYSize);


    EXPECT_GT(darkStd, 1.0) << "Dark noise too low";
    EXPECT_GT(brightStd, 1.0) << "Bright noise too low";

    EXPECT_GE(brightStd, darkStd * 0.8)
        << "Bright noise (" << brightStd << ") should be >= dark noise (" << darkStd << ")";
}


TEST_F(NoiseOverlayTest, ValuesClampedTo0_255) {
    NoiseOverlay noise(5.0f, 0.5f, 0x9999);


    auto dark = makeUniform(2);
    noise.apply(dark.data(), kWidth, kHeight, 8.0f);
    for (int i = 0; i < kNV21Size; i++) {
        EXPECT_GE(dark[i], 0);
        EXPECT_LE(dark[i], 255);
    }


    NoiseOverlay noise2(5.0f, 0.5f, 0x8888);
    auto bright = makeUniform(253);
    noise2.apply(bright.data(), kWidth, kHeight, 8.0f);
    for (int i = 0; i < kNV21Size; i++) {
        EXPECT_GE(bright[i], 0);
        EXPECT_LE(bright[i], 255);
    }
}


TEST_F(NoiseOverlayTest, FPNStableWithSameSeed) {
    uint64_t seed = 0xDEADBEEF42ULL;


    NoiseOverlay noise1(0.0f, 0.0f, seed);
    NoiseOverlay noise2(0.0f, 0.0f, seed);

    auto buf1 = makeUniform(128);
    auto buf2 = makeUniform(128);

    noise1.applyLuma(buf1.data(), kWidth, kHeight, 1.0f);
    noise2.applyLuma(buf2.data(), kWidth, kHeight, 1.0f);


    int matching = 0;
    for (int i = 0; i < kYSize; i++) {
        if (buf1[i] == buf2[i]) matching++;
    }

    EXPECT_EQ(matching, kYSize)
        << "FPN not stable: " << (kYSize - matching) << " pixels differ";
}


TEST_F(NoiseOverlayTest, HigherISOMoreNoise) {
    NoiseOverlay noiseLow(3.0f, 0.35f, 0x1111);
    NoiseOverlay noiseHigh(3.0f, 0.35f, 0x1111);

    auto low = makeUniform(128);
    auto lowOrig = low;
    noiseLow.applyLuma(low.data(), kWidth, kHeight, 1.0f);
    double lowStd = stddev(low.data(), lowOrig.data(), kYSize);

    auto high = makeUniform(128);
    auto highOrig = high;
    noiseHigh.applyLuma(high.data(), kWidth, kHeight, 16.0f);
    double highStd = stddev(high.data(), highOrig.data(), kYSize);

    EXPECT_GT(highStd, lowStd * 1.5)
        << "High ISO noise (" << highStd << ") should be much more than low ISO (" << lowStd << ")";
}


TEST_F(NoiseOverlayTest, FPNUniquePerDevice) {
    NoiseOverlay noisePureFPN1(0.0f, 0.0f);
    noisePureFPN1.setSensorFingerprint("AAA111", "Pixel 7");
    NoiseOverlay noisePureFPN2(0.0f, 0.0f);
    noisePureFPN2.setSensorFingerprint("BBB222", "Pixel 7");

    std::vector<uint8_t> buf1(kNV21Size, 128);
    std::vector<uint8_t> buf2(kNV21Size, 128);

    noisePureFPN1.apply(buf1.data(), kWidth, kHeight, 0.0f);
    noisePureFPN2.apply(buf2.data(), kWidth, kHeight, 0.0f);


    int diff = 0;
    for (int i = 0; i < kWidth * kHeight; i++) {
        if (buf1[i] != buf2[i]) diff++;
    }
    EXPECT_GT(diff, 10) << "Different serials should produce different FPN patterns";
}


TEST_F(NoiseOverlayTest, FPNStableForSameSerial) {
    NoiseOverlay noise1(0.0f, 0.0f);
    noise1.setSensorFingerprint("SERIAL001", "Pixel 7");
    NoiseOverlay noise2(0.0f, 0.0f);
    noise2.setSensorFingerprint("SERIAL001", "Pixel 7");

    std::vector<uint8_t> buf1(kNV21Size, 128);
    std::vector<uint8_t> buf2(kNV21Size, 128);

    noise1.apply(buf1.data(), kWidth, kHeight, 0.0f);
    noise2.apply(buf2.data(), kWidth, kHeight, 0.0f);


    int matching = 0;
    for (int i = 0; i < kWidth * kHeight; i++) {
        if (buf1[i] == buf2[i]) matching++;
    }
    EXPECT_EQ(matching, kWidth * kHeight)
        << "Same serial should produce identical FPN patterns";
}


TEST_F(NoiseOverlayTest, RowFPNApplied) {

    auto buf = std::vector<uint8_t>(kNV21Size, 128);

    NoiseOverlay noise(0.001f, 0.0f, 0x12345678);
    noise.apply(buf.data(), kWidth, kHeight, 0.001f);


    std::vector<float> rowMeans(kHeight, 0.0f);
    for (int y = 0; y < kHeight; y++) {
        float sum = 0;
        for (int x = 0; x < kWidth; x++) sum += buf[y * kWidth + x];
        rowMeans[y] = sum / kWidth;
    }

    float minMean = *std::min_element(rowMeans.begin(), rowMeans.end());
    float maxMean = *std::max_element(rowMeans.begin(), rowMeans.end());


    EXPECT_GT(maxMean - minMean, 0.1f) << "Row FPN should cause row-to-row variation";
}


TEST_F(NoiseOverlayTest, HotDeadPixelsPresent) {
    auto buf = std::vector<uint8_t>(kNV21Size, 128);
    NoiseOverlay noise(0.0f, 0.0f, 0xABCDEF12);
    noise.apply(buf.data(), kWidth, kHeight, 0.0f);

    int hotCount  = 0;
    int deadCount = 0;
    for (int i = 0; i < kWidth * kHeight; i++) {
        if (buf[i] == 255) hotCount++;
        if (buf[i] == 0)   deadCount++;
    }

    EXPECT_GE(hotCount, 1)  << "Should have at least 1 hot pixel";
    EXPECT_GE(deadCount, 1) << "Should have at least 1 dead pixel";
}
