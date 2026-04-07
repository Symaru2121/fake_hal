

#include <gtest/gtest.h>
#include "GrallocHelper.h"
#include "JpegEncoder.h"
#include "NoiseOverlay.h"
#include <hardware/camera3.h>

#include <sys/mman.h>
#include <cstring>
#include <vector>


#include <sys/syscall.h>
#include <unistd.h>


#include <jpeglib.h>

using namespace fake_hal;


static native_handle_t* createTestBufferHandle(size_t size) {

    int fd = (int)syscall(SYS_memfd_create, "test_gralloc_buf", 0u);
    if (fd < 0) return nullptr;


    if (ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        return nullptr;
    }


    native_handle_t* nh = native_handle_create(1, 0);
    if (!nh) { close(fd); return nullptr; }
    nh->data[0] = fd;

    return nh;
}

static void destroyTestBufferHandle(native_handle_t* nh) {
    if (nh) {
        if (nh->numFds > 0 && nh->data[0] >= 0) close(nh->data[0]);
        native_handle_delete(nh);
    }
}


class GrallocIntegrationTest : public ::testing::Test {
protected:
    static constexpr int kWidth  = 640;
    static constexpr int kHeight = 480;
    static constexpr size_t kYUVSize = kWidth * kHeight * 3 / 2;
};


TEST_F(GrallocIntegrationTest, WriteYUVToBuffer_FullRoundtrip) {
    auto* nh = createTestBufferHandle(kYUVSize);
    ASSERT_NE(nh, nullptr) << "Failed to create memfd handle";

    buffer_handle_t handle = nh;


    std::vector<uint8_t> nv21(kYUVSize);
    for (int y = 0; y < kHeight; y++) {
        for (int x = 0; x < kWidth; x++) {
            nv21[(size_t)(y * kWidth + x)] = (uint8_t)((x + y) % 256);
        }
    }

    memset(nv21.data() + kWidth * kHeight, 128, (size_t)(kWidth * kHeight / 2));


    NoiseOverlay noise(3.0f, 0.35f, 0xCAFE);
    noise.apply(nv21.data(), kWidth, kHeight, 1.0f);


    auto& gralloc = GrallocHelper::getInstance();
    void* ptr = nullptr;
    ASSERT_TRUE(gralloc.lock(handle, kWidth, kHeight,
                             GRALLOC_USAGE_SW_WRITE_OFTEN, &ptr))
        << "gralloc lock failed (version: " << gralloc.versionString() << ")";
    ASSERT_NE(ptr, nullptr);


    memcpy(ptr, nv21.data(), kYUVSize);


    int fence = gralloc.unlock(handle);
    (void)fence;


    void* readback = mmap(nullptr, kYUVSize, PROT_READ, MAP_SHARED, nh->data[0], 0);
    ASSERT_NE(readback, MAP_FAILED);


    int mismatches = 0;
    uint8_t* rb = (uint8_t*)readback;
    for (size_t i = 0; i < kYUVSize; i++) {
        if (rb[i] != nv21[i]) mismatches++;
    }
    EXPECT_EQ(mismatches, 0) << "Data mismatch after gralloc write/readback";


    printf("  First 8 Y pixels written:  ");
    for (int i = 0; i < 8; i++) printf("%3d ", nv21[i]);
    printf("\n  First 8 Y pixels readback: ");
    for (int i = 0; i < 8; i++) printf("%3d ", rb[i]);
    printf("\n");

    munmap(readback, kYUVSize);
    destroyTestBufferHandle(nh);
}


TEST_F(GrallocIntegrationTest, WriteJPEGToBuffer_WithCameraBlobTrailer) {

    int blobBufSize = kWidth * kHeight * 3;

    auto* nh = createTestBufferHandle((size_t)blobBufSize);
    ASSERT_NE(nh, nullptr);
    buffer_handle_t handle = nh;


    std::vector<uint8_t> nv21(kYUVSize, 128);
    for (int y = 0; y < kHeight; y++)
        for (int x = 0; x < kWidth; x++)
            nv21[(size_t)(y * kWidth + x)] = (uint8_t)((x * 2 + y) % 256);


    JpegEncoder encoder;
    JpegEncoder::ExifData exif;
    exif.iso = 200;
    exif.imageWidth = kWidth;
    exif.imageHeight = kHeight;

    std::vector<uint8_t> jpeg;
    ASSERT_TRUE(encoder.encode(nv21.data(), kWidth, kHeight, 95, exif, jpeg));
    ASSERT_GT(jpeg.size(), 100u);


    auto& gralloc = GrallocHelper::getInstance();
    void* ptr = nullptr;
    ASSERT_TRUE(gralloc.lock(handle, blobBufSize, 1,
                             GRALLOC_USAGE_SW_WRITE_OFTEN, &ptr));


    uint8_t* buf = (uint8_t*)ptr;
    memcpy(buf, jpeg.data(), jpeg.size());


    CameraBlob* trailer = (CameraBlob*)(buf + blobBufSize - sizeof(CameraBlob));
    trailer->blobId   = CAMERA_BLOB_ID_JPEG;
    trailer->blobSize = (uint32_t)jpeg.size();


    gralloc.unlock(handle);


    void* rb = mmap(nullptr, (size_t)blobBufSize, PROT_READ, MAP_SHARED, nh->data[0], 0);
    ASSERT_NE(rb, MAP_FAILED);

    uint8_t* rbuf = (uint8_t*)rb;


    EXPECT_EQ(rbuf[0], 0xFF);
    EXPECT_EQ(rbuf[1], 0xD8);


    CameraBlob* readTrailer = (CameraBlob*)(rbuf + blobBufSize - sizeof(CameraBlob));
    EXPECT_EQ(readTrailer->blobId, (uint32_t)CAMERA_BLOB_ID_JPEG);
    EXPECT_EQ(readTrailer->blobSize, (uint32_t)jpeg.size());


    EXPECT_EQ(memcmp(rbuf, jpeg.data(), jpeg.size()), 0);

    printf("  JPEG size: %zu bytes, blob buffer: %d bytes\n", jpeg.size(), blobBufSize);
    printf("  CameraBlob trailer: blobId=0x%04X, blobSize=%u\n",
           readTrailer->blobId, readTrailer->blobSize);

    munmap(rb, (size_t)blobBufSize);
    destroyTestBufferHandle(nh);
}


TEST_F(GrallocIntegrationTest, MultipleLocksNoLeak) {
    auto* nh = createTestBufferHandle(kYUVSize);
    ASSERT_NE(nh, nullptr);
    buffer_handle_t handle = nh;

    auto& gralloc = GrallocHelper::getInstance();

    for (int i = 0; i < 100; i++) {
        void* ptr = nullptr;
        ASSERT_TRUE(gralloc.lock(handle, kWidth, kHeight,
                                 GRALLOC_USAGE_SW_WRITE_OFTEN, &ptr))
            << "Lock failed on iteration " << i;
        ASSERT_NE(ptr, nullptr);


        ((uint8_t*)ptr)[0] = (uint8_t)(i % 256);

        gralloc.unlock(handle);
    }


    destroyTestBufferHandle(nh);
}


TEST_F(GrallocIntegrationTest, NullHandleReturnsFalse) {
    auto& gralloc = GrallocHelper::getInstance();
    void* ptr = nullptr;
    EXPECT_FALSE(gralloc.lock(nullptr, kWidth, kHeight,
                              GRALLOC_USAGE_SW_WRITE_OFTEN, &ptr));
    EXPECT_EQ(ptr, nullptr);
}


TEST_F(GrallocIntegrationTest, FullPipelineToGrallocToJPEGDecode) {


    std::vector<uint8_t> nv21(kYUVSize);
    for (int i = 0; i < kWidth * kHeight; i++)
        nv21[i] = (uint8_t)(i % 200 + 28);
    memset(nv21.data() + kWidth * kHeight, 128, (size_t)(kWidth * kHeight / 2));


    NoiseOverlay noise(3.0f, 0.35f, 0xBEEF);
    noise.apply(nv21.data(), kWidth, kHeight, 1.5f);


    auto* yuvNH = createTestBufferHandle(kYUVSize);
    ASSERT_NE(yuvNH, nullptr);

    auto& gralloc = GrallocHelper::getInstance();
    void* yuvPtr = nullptr;
    ASSERT_TRUE(gralloc.lock(yuvNH, kWidth, kHeight,
                             GRALLOC_USAGE_SW_WRITE_OFTEN, &yuvPtr));
    memcpy(yuvPtr, nv21.data(), kYUVSize);
    gralloc.unlock(yuvNH);


    void* yuvRB = mmap(nullptr, kYUVSize, PROT_READ, MAP_SHARED, yuvNH->data[0], 0);
    ASSERT_NE(yuvRB, MAP_FAILED);


    JpegEncoder enc;
    JpegEncoder::ExifData exif;
    exif.iso = 150;
    exif.imageWidth = kWidth;
    exif.imageHeight = kHeight;

    std::vector<uint8_t> jpeg;
    ASSERT_TRUE(enc.encode((uint8_t*)yuvRB, kWidth, kHeight, 90, exif, jpeg));


    struct jpeg_decompress_struct dinfo;
    struct jpeg_error_mgr jerr;
    dinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&dinfo);
    jpeg_mem_src(&dinfo, jpeg.data(), jpeg.size());
    ASSERT_EQ(jpeg_read_header(&dinfo, TRUE), JPEG_HEADER_OK);
    EXPECT_EQ((int)dinfo.image_width, kWidth);
    EXPECT_EQ((int)dinfo.image_height, kHeight);
    jpeg_destroy_decompress(&dinfo);

    printf("  Pipeline: NV21(%dx%d) -> noise -> gralloc -> readback -> JPEG(%zu bytes) -> decode OK\n",
           kWidth, kHeight, jpeg.size());

    munmap(yuvRB, kYUVSize);
    destroyTestBufferHandle(yuvNH);
}
