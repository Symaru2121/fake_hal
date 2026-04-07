#include <gtest/gtest.h>
#include "GyroWarp.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

using namespace fake_hal;

class GyroWarpTest : public ::testing::Test {
protected:
    static constexpr int kWidth  = 64;
    static constexpr int kHeight = 48;
    static constexpr int kYSize  = kWidth * kHeight;
    static constexpr int kNV21Size = kYSize * 3 / 2;


    std::vector<uint8_t> makeGradient() {
        std::vector<uint8_t> buf(kNV21Size, 128);
        for (int row = 0; row < kHeight; row++) {
            for (int col = 0; col < kWidth; col++) {

                buf[row * kWidth + col] = (uint8_t)(col * 255 / (kWidth - 1));
            }
        }

        return buf;
    }
};


TEST_F(GyroWarpTest, ZeroAnglesNoChange) {

    GyroWarp warp("/nonexistent/iio/device", 40, 20.0f);


    auto buf = makeGradient();
    auto original = buf;
    std::vector<uint8_t> tmp(kNV21Size);

    warp.apply(buf.data(), kWidth, kHeight, tmp.data());


    EXPECT_EQ(buf, original) << "Image changed despite zero angles";
}


TEST_F(GyroWarpTest, ShiftRightMovesPixels) {
    auto src = makeGradient();
    std::vector<uint8_t> dst(kNV21Size, 0);

    int dx = 5, dy = 0;
    GyroWarp::shiftNV21(src.data(), dst.data(), kWidth, kHeight, dx, dy);


    for (int row = 0; row < kHeight; row++) {
        for (int col = dx; col < kWidth; col++) {
            int srcCol = col - dx;
            EXPECT_EQ(dst[row * kWidth + col], src[row * kWidth + srcCol])
                << "Mismatch at (" << row << "," << col << ")";
        }
    }
}


TEST_F(GyroWarpTest, ShiftProportionalToAngle) {

    auto src = makeGradient();


    std::vector<uint8_t> dst1(kNV21Size, 0);
    GyroWarp::shiftNV21(src.data(), dst1.data(), kWidth, kHeight, 2, 0);


    std::vector<uint8_t> dst2(kNV21Size, 0);
    GyroWarp::shiftNV21(src.data(), dst2.data(), kWidth, kHeight, 10, 0);


    int testRow = kHeight / 2;
    int testCol = kWidth / 2;


    uint8_t expected1 = src[testRow * kWidth + (testCol - 2)];
    uint8_t expected2 = src[testRow * kWidth + (testCol - 10)];

    EXPECT_EQ(dst1[testRow * kWidth + testCol], expected1);
    EXPECT_EQ(dst2[testRow * kWidth + testCol], expected2);
    EXPECT_NE(expected1, expected2) << "Source values should differ for different offsets";
}


TEST_F(GyroWarpTest, BoundaryClampsToEdge) {
    auto src = makeGradient();
    std::vector<uint8_t> dst(kNV21Size, 0);

    int dx = kWidth + 10;
    GyroWarp::shiftNV21(src.data(), dst.data(), kWidth, kHeight, dx, 0);


    int testRow = kHeight / 2;
    uint8_t edgeVal = src[testRow * kWidth + 0];
    for (int col = 0; col < kWidth; col++) {
        EXPECT_EQ(dst[testRow * kWidth + col], edgeVal)
            << "Expected edge clamping at col " << col;
    }
}


TEST_F(GyroWarpTest, NegativeShiftMovesLeft) {
    auto src = makeGradient();
    std::vector<uint8_t> dst(kNV21Size, 0);

    int dx = -5;
    GyroWarp::shiftNV21(src.data(), dst.data(), kWidth, kHeight, dx, 0);


    int testRow = kHeight / 2;
    for (int col = 0; col < kWidth - 5; col++) {
        EXPECT_EQ(dst[testRow * kWidth + col], src[testRow * kWidth + (col + 5)])
            << "Mismatch at col " << col;
    }
}


TEST_F(GyroWarpTest, VerticalShiftWorks) {

    std::vector<uint8_t> src(kNV21Size, 128);
    for (int row = 0; row < kHeight; row++) {
        for (int col = 0; col < kWidth; col++) {
            src[row * kWidth + col] = (uint8_t)(row * 255 / (kHeight - 1));
        }
    }

    std::vector<uint8_t> dst(kNV21Size, 0);
    int dy = 3;
    GyroWarp::shiftNV21(src.data(), dst.data(), kWidth, kHeight, 0, dy);


    int testCol = kWidth / 2;
    for (int row = dy; row < kHeight; row++) {
        EXPECT_EQ(dst[row * kWidth + testCol], src[(row - dy) * kWidth + testCol])
            << "Mismatch at row " << row;
    }
}


TEST_F(GyroWarpTest, UVPlaneShifts) {

    std::vector<uint8_t> src(kNV21Size);
    std::memset(src.data(), 128, kYSize);

    uint8_t* uvSrc = src.data() + kYSize;
    int uvWidth = kWidth / 2;
    int uvHeight = kHeight / 2;
    for (int row = 0; row < uvHeight; row++) {
        for (int col = 0; col < uvWidth; col++) {
            uvSrc[(row * uvWidth + col) * 2 + 0] = (uint8_t)(col * 2);
            uvSrc[(row * uvWidth + col) * 2 + 1] = (uint8_t)(col * 2 + 1);
        }
    }

    std::vector<uint8_t> dst(kNV21Size, 0);
    int dx = 4;
    GyroWarp::shiftNV21(src.data(), dst.data(), kWidth, kHeight, dx, 0);


    const uint8_t* uvDst = dst.data() + kYSize;
    int dxUV = dx / 2;
    int testRow = uvHeight / 2;
    for (int col = dxUV; col < uvWidth; col++) {
        int srcCol = col - dxUV;
        EXPECT_EQ(uvDst[(testRow * uvWidth + col) * 2 + 0],
                  uvSrc[(testRow * uvWidth + srcCol) * 2 + 0])
            << "UV V mismatch at col " << col;
        EXPECT_EQ(uvDst[(testRow * uvWidth + col) * 2 + 1],
                  uvSrc[(testRow * uvWidth + srcCol) * 2 + 1])
            << "UV U mismatch at col " << col;
    }
}
