

#include <gtest/gtest.h>

#include "FakeCameraProvider.h"
#include "FakeCameraDevice.h"
#include "VideoFrameReader.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace fake_hal;
using namespace aidl::android::hardware::camera::device;
using namespace aidl::android::hardware::camera::provider;
using namespace aidl::android::hardware::camera::common;


class MockProviderCallback : public ICameraProviderCallback {
public:
    struct DeviceStatusEvent {
        std::string cameraDeviceName;
        CameraDeviceStatus status;
    };
    struct TorchStatusEvent {
        std::string cameraDeviceName;
        TorchModeStatus status;
    };

    std::vector<DeviceStatusEvent> deviceStatusEvents;
    std::vector<TorchStatusEvent> torchStatusEvents;
    std::mutex mu;

    ndk::ScopedAStatus cameraDeviceStatusChange(
        const std::string& cameraDeviceName,
        CameraDeviceStatus newStatus) override
    {
        std::lock_guard<std::mutex> lk(mu);
        deviceStatusEvents.push_back({cameraDeviceName, newStatus});
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus torchModeStatusChange(
        const std::string& cameraDeviceName,
        TorchModeStatus newStatus) override
    {
        std::lock_guard<std::mutex> lk(mu);
        torchStatusEvents.push_back({cameraDeviceName, newStatus});
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus physicalCameraDeviceStatusChange(
        const std::string&, const std::string&,
        CameraDeviceStatus) override
    {
        return ndk::ScopedAStatus::ok();
    }
};


class MockDeviceCallback : public ICameraDeviceCallback {
public:
    std::vector<CaptureResult> results;
    std::mutex mu;
    std::condition_variable cv;

    ndk::ScopedAStatus processCaptureResult(
        const std::vector<CaptureResult>& newResults) override
    {
        std::lock_guard<std::mutex> lk(mu);
        for (auto& r : newResults) {
            results.push_back(r);
        }
        cv.notify_all();
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus notify(const std::vector<NotifyMsg>&) override {
        return ndk::ScopedAStatus::ok();
    }


    bool waitForResults(size_t count, int timeoutMs = 5000) {
        std::unique_lock<std::mutex> lk(mu);
        return cv.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                           [&] { return results.size() >= count; });
    }
};


class HALIntegrationTest : public ::testing::Test {
protected:


    const std::string kVideoPath = "/tmp/test_video_nonexistent.mp4";

    std::shared_ptr<FakeCameraProvider> provider;
    std::shared_ptr<MockProviderCallback> providerCallback;

    void SetUp() override {
        provider = std::make_shared<FakeCameraProvider>(kVideoPath);
        providerCallback = std::make_shared<MockProviderCallback>();
    }

    void TearDown() override {
        provider.reset();
        providerCallback.reset();
    }


    std::shared_ptr<ICameraDevice> getDevice(const std::string& id) {
        std::shared_ptr<ICameraDevice> device;
        auto status = provider->getCameraDeviceInterface(id, &device);
        EXPECT_TRUE(status.isOk());
        EXPECT_NE(device, nullptr);
        return device;
    }


    std::pair<std::shared_ptr<ICameraDeviceSession>, std::shared_ptr<MockDeviceCallback>>
    openSession(std::shared_ptr<ICameraDevice>& device) {
        auto cb = std::make_shared<MockDeviceCallback>();
        std::shared_ptr<ICameraDeviceSession> session;
        auto status = device->open(cb, &session);
        EXPECT_TRUE(status.isOk());
        EXPECT_NE(session, nullptr);
        return {session, cb};
    }


    std::vector<HalStream> configureDefaultStreams(
        std::shared_ptr<ICameraDeviceSession>& session)
    {
        StreamConfiguration config;

        Stream yuvStream;
        yuvStream.id = 0;
        yuvStream.format = HAL_PIXEL_FORMAT_YCbCr_420_888;
        yuvStream.width = 640;
        yuvStream.height = 480;
        yuvStream.streamType = StreamType::OUTPUT;
        config.streams.push_back(yuvStream);

        Stream blobStream;
        blobStream.id = 1;
        blobStream.format = HAL_PIXEL_FORMAT_BLOB;
        blobStream.width = 1920;
        blobStream.height = 1080;
        blobStream.streamType = StreamType::OUTPUT;
        config.streams.push_back(blobStream);

        std::vector<HalStream> halStreams;
        auto status = session->configureStreams(config, &halStreams);
        EXPECT_TRUE(status.isOk());
        return halStreams;
    }


    CaptureRequest buildRequest(int32_t frameNum) {
        CaptureRequest req;
        req.frameNumber = frameNum;

        StreamBuffer buf0;
        buf0.streamId = 0;
        buf0.bufferId = frameNum * 10;
        buf0.buffer = nullptr;
        buf0.status = BufferStatus::OK;
        req.outputBuffers.push_back(buf0);

        StreamBuffer buf1;
        buf1.streamId = 1;
        buf1.bufferId = frameNum * 10 + 1;
        buf1.buffer = nullptr;
        buf1.status = BufferStatus::OK;
        req.outputBuffers.push_back(buf1);

        return req;
    }
};


TEST_F(HALIntegrationTest, SetCallbackSucceeds) {
    auto status = provider->setCallback(providerCallback);
    EXPECT_TRUE(status.isOk());
}


TEST_F(HALIntegrationTest, GetCameraIdListReturnsTwoCameras) {
    std::vector<std::string> ids;
    auto status = provider->getCameraIdList(&ids);
    ASSERT_TRUE(status.isOk());
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "0");
    EXPECT_EQ(ids[1], "1");
}


TEST_F(HALIntegrationTest, GetCameraDeviceInterface) {
    auto device0 = getDevice("0");
    ASSERT_NE(device0, nullptr);

    auto device1 = getDevice("1");
    ASSERT_NE(device1, nullptr);


    std::shared_ptr<ICameraDevice> deviceBad;
    auto status = provider->getCameraDeviceInterface("99", &deviceBad);
    EXPECT_FALSE(status.isOk());
}


TEST_F(HALIntegrationTest, CameraCharacteristicsBack) {
    auto device = getDevice("0");
    ASSERT_NE(device, nullptr);

    android::CameraMetadata chars;
    auto status = device->getCameraCharacteristics(&chars);
    ASSERT_TRUE(status.isOk());


    EXPECT_TRUE(chars.exists(ANDROID_LENS_FACING));
    auto* facingEntry = chars.find(ANDROID_LENS_FACING);
    ASSERT_NE(facingEntry, nullptr);
    EXPECT_EQ(facingEntry->data[0], ANDROID_LENS_FACING_BACK);


    EXPECT_TRUE(chars.exists(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE));
    int32_t pixelArray[2] = {};
    auto* paEntry = chars.find(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE);
    ASSERT_NE(paEntry, nullptr);
    ASSERT_EQ(paEntry->count, 2u);
    memcpy(pixelArray, paEntry->data.data(), sizeof(pixelArray));
    EXPECT_EQ(pixelArray[0], 4080);
    EXPECT_EQ(pixelArray[1], 3072);


    EXPECT_TRUE(chars.exists(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL));
    auto* hwEntry = chars.find(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL);
    ASSERT_NE(hwEntry, nullptr);
    EXPECT_EQ(hwEntry->data[0], ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL);
}


TEST_F(HALIntegrationTest, CameraCharacteristicsFront) {
    auto device = getDevice("1");
    ASSERT_NE(device, nullptr);

    android::CameraMetadata chars;
    auto status = device->getCameraCharacteristics(&chars);
    ASSERT_TRUE(status.isOk());


    auto* facingEntry = chars.find(ANDROID_LENS_FACING);
    ASSERT_NE(facingEntry, nullptr);
    EXPECT_EQ(facingEntry->data[0], ANDROID_LENS_FACING_FRONT);
}


TEST_F(HALIntegrationTest, OpenSessionAndConfigureStreams) {
    auto device = getDevice("0");
    auto [session, cb] = openSession(device);
    ASSERT_NE(session, nullptr);

    auto halStreams = configureDefaultStreams(session);
    ASSERT_EQ(halStreams.size(), 2u);


    EXPECT_EQ(halStreams[0].id, 0);
    EXPECT_EQ(halStreams[1].id, 1);


    EXPECT_GT(halStreams[0].maxBuffers, 0);
    EXPECT_GT(halStreams[1].maxBuffers, 0);


    session->close();
}


TEST_F(HALIntegrationTest, ProcessCaptureRequestAndReceiveResults) {
    auto device = getDevice("0");
    auto [session, cb] = openSession(device);
    configureDefaultStreams(session);


    const int kNumRequests = 3;
    for (int i = 0; i < kNumRequests; i++) {
        std::vector<CaptureRequest> requests = { buildRequest(i) };
        std::vector<BufferCache> caches;
        int32_t numProcessed = 0;
        auto status = session->processCaptureRequest(requests, caches, &numProcessed);
        EXPECT_TRUE(status.isOk());
        EXPECT_EQ(numProcessed, 1);
    }


    ASSERT_TRUE(cb->waitForResults(kNumRequests, 10000))
        << "Timed out waiting for " << kNumRequests << " capture results";


    {
        std::lock_guard<std::mutex> lk(cb->mu);
        ASSERT_GE(cb->results.size(), (size_t)kNumRequests);


        for (size_t i = 0; i < (size_t)kNumRequests; i++) {
            EXPECT_EQ(cb->results[i].frameNumber, (int32_t)i);
        }


        for (size_t i = 0; i < (size_t)kNumRequests; i++) {
            EXPECT_EQ(cb->results[i].outputBuffers.size(), 2u);
        }


        for (size_t i = 0; i < (size_t)kNumRequests; i++) {
            EXPECT_EQ(cb->results[i].partialResult, 1);
        }
    }

    session->close();
}


TEST_F(HALIntegrationTest, CaptureResultMetadataValid) {
    auto device = getDevice("0");
    auto [session, cb] = openSession(device);
    configureDefaultStreams(session);


    const int kNumRequests = 2;
    for (int i = 0; i < kNumRequests; i++) {
        std::vector<CaptureRequest> requests = { buildRequest(i) };
        std::vector<BufferCache> caches;
        int32_t numProcessed = 0;
        session->processCaptureRequest(requests, caches, &numProcessed);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(cb->waitForResults(kNumRequests, 10000));


    {
        std::lock_guard<std::mutex> lk(cb->mu);
        ASSERT_GE(cb->results.size(), 2u);

        EXPECT_EQ(cb->results[0].frameNumber, 0);
        EXPECT_EQ(cb->results[1].frameNumber, 1);
    }


    MetadataRandomizer metaRand;
    android::CameraMetadata meta0, meta1;

    int64_t ts0 = 1'000'000'000LL;
    int64_t ts1 = 1'033'333'333LL;
    metaRand.fill(0, &meta0, ts0);
    metaRand.fill(1, &meta1, ts1);


    int64_t readTs0 = 0, readTs1 = 0;
    ASSERT_TRUE(meta0.getInt64(ANDROID_SENSOR_TIMESTAMP, &readTs0));
    ASSERT_TRUE(meta1.getInt64(ANDROID_SENSOR_TIMESTAMP, &readTs1));
    EXPECT_EQ(readTs0, ts0);
    EXPECT_EQ(readTs1, ts1);
    EXPECT_GT(readTs1, readTs0) << "Timestamps should be monotonically increasing";


    int32_t iso0 = 0, iso1 = 0;
    ASSERT_TRUE(meta0.getInt32(ANDROID_SENSOR_SENSITIVITY, &iso0));
    ASSERT_TRUE(meta1.getInt32(ANDROID_SENSOR_SENSITIVITY, &iso1));
    EXPECT_GE(iso0, 50);
    EXPECT_LE(iso0, 3200);
    EXPECT_GE(iso1, 50);
    EXPECT_LE(iso1, 3200);

    session->close();
}


TEST_F(HALIntegrationTest, FlushReturnsOK) {
    auto device = getDevice("0");
    auto [session, cb] = openSession(device);
    configureDefaultStreams(session);

    auto status = session->flush();
    EXPECT_TRUE(status.isOk());

    session->close();
}


TEST_F(HALIntegrationTest, CloseSessionCleanly) {
    auto device = getDevice("0");
    auto [session, cb] = openSession(device);
    configureDefaultStreams(session);


    std::vector<CaptureRequest> requests = { buildRequest(0) };
    std::vector<BufferCache> caches;
    int32_t numProcessed = 0;
    session->processCaptureRequest(requests, caches, &numProcessed);


    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto status = session->close();
    EXPECT_TRUE(status.isOk());
}


TEST_F(HALIntegrationTest, SequentialSessions) {
    auto device = getDevice("0");


    {
        auto [session1, cb1] = openSession(device);
        configureDefaultStreams(session1);

        std::vector<CaptureRequest> requests = { buildRequest(0) };
        std::vector<BufferCache> caches;
        int32_t numProcessed = 0;
        session1->processCaptureRequest(requests, caches, &numProcessed);
        EXPECT_EQ(numProcessed, 1);

        ASSERT_TRUE(cb1->waitForResults(1, 5000));
        {
            std::lock_guard<std::mutex> lk(cb1->mu);
            EXPECT_GE(cb1->results.size(), 1u);
        }

        session1->close();
    }


    {
        auto [session2, cb2] = openSession(device);
        configureDefaultStreams(session2);

        std::vector<CaptureRequest> requests = { buildRequest(0) };
        std::vector<BufferCache> caches;
        int32_t numProcessed = 0;
        session2->processCaptureRequest(requests, caches, &numProcessed);
        EXPECT_EQ(numProcessed, 1);

        ASSERT_TRUE(cb2->waitForResults(1, 5000));
        {
            std::lock_guard<std::mutex> lk(cb2->mu);
            EXPECT_GE(cb2->results.size(), 1u);
        }

        session2->close();
    }
}


TEST_F(HALIntegrationTest, IndependentCameraSessions) {
    auto device0 = getDevice("0");
    auto device1 = getDevice("1");

    auto [session0, cb0] = openSession(device0);
    auto [session1, cb1] = openSession(device1);

    configureDefaultStreams(session0);
    configureDefaultStreams(session1);


    {
        std::vector<CaptureRequest> req0 = { buildRequest(0) };
        std::vector<BufferCache> caches;
        int32_t n = 0;
        session0->processCaptureRequest(req0, caches, &n);
        EXPECT_EQ(n, 1);
    }
    {
        std::vector<CaptureRequest> req1 = { buildRequest(0) };
        std::vector<BufferCache> caches;
        int32_t n = 0;
        session1->processCaptureRequest(req1, caches, &n);
        EXPECT_EQ(n, 1);
    }

    ASSERT_TRUE(cb0->waitForResults(1, 5000));
    ASSERT_TRUE(cb1->waitForResults(1, 5000));

    {
        std::lock_guard<std::mutex> lk(cb0->mu);
        EXPECT_GE(cb0->results.size(), 1u);
    }
    {
        std::lock_guard<std::mutex> lk(cb1->mu);
        EXPECT_GE(cb1->results.size(), 1u);
    }

    session0->close();
    session1->close();
}


TEST_F(HALIntegrationTest, GetVendorTagsEmpty) {
    std::vector<VendorTagSection> vts;
    auto status = provider->getVendorTags(&vts);
    ASSERT_TRUE(status.isOk());
    EXPECT_TRUE(vts.empty());
}


TEST_F(HALIntegrationTest, NotifyDeviceStateChange) {
    auto status = provider->notifyDeviceStateChange(0x1);
    EXPECT_TRUE(status.isOk());
}


TEST_F(HALIntegrationTest, GetResourceCost) {
    auto device = getDevice("0");
    CameraResourceCost cost;
    auto status = device->getResourceCost(&cost);
    ASSERT_TRUE(status.isOk());
    EXPECT_EQ(cost.resourceCost, 50);
    EXPECT_TRUE(cost.conflictingDevices.empty());
}


TEST_F(HALIntegrationTest, StreamCombinationSupported) {
    auto device = getDevice("0");
    StreamConfiguration config;
    bool support = false;
    auto status = device->isStreamCombinationSupported(config, &support);
    ASSERT_TRUE(status.isOk());
    EXPECT_TRUE(support);
}


TEST_F(HALIntegrationTest, SetTorchMode) {
    auto device = getDevice("0");
    EXPECT_TRUE(device->setTorchMode(true).isOk());
    EXPECT_TRUE(device->setTorchMode(false).isOk());
}


class VideoReaderFallbackTest : public ::testing::Test {};

TEST_F(VideoReaderFallbackTest, OpenNonexistentFileFails) {
    VideoFrameReader reader("/nonexistent_path/video.mp4");


    EXPECT_FALSE(reader.open(640, 480));
    EXPECT_FALSE(reader.isOpen());
}

TEST_F(VideoReaderFallbackTest, LastFrameBeforeOpenReturnsEmpty) {
    VideoFrameReader reader("/nonexistent_path/video.mp4");
    reader.open(640, 480);


    std::vector<uint8_t> buf(640 * 480 * 3 / 2, 0);
    EXPECT_FALSE(reader.lastFrame(buf.data()));
}

TEST_F(VideoReaderFallbackTest, NextFrameBeforeOpenReturnsFalse) {
    VideoFrameReader reader("/nonexistent_path/video.mp4");
    reader.open(640, 480);

    std::vector<uint8_t> buf(640 * 480 * 3 / 2, 0);
    EXPECT_FALSE(reader.nextFrame(buf.data()));
}


class HALPerformanceTest : public ::testing::Test {
protected:
    const std::string kVideoPath = "/tmp/test_video_perf.mp4";
};

TEST_F(HALPerformanceTest, CaptureRequestThroughput) {
    auto provider = std::make_shared<FakeCameraProvider>(kVideoPath);

    std::shared_ptr<ICameraDevice> device;
    auto st = provider->getCameraDeviceInterface("0", &device);
    ASSERT_TRUE(st.isOk());
    ASSERT_NE(device, nullptr);

    auto cb = std::make_shared<MockDeviceCallback>();
    std::shared_ptr<ICameraDeviceSession> session;
    st = device->open(cb, &session);
    ASSERT_TRUE(st.isOk());
    ASSERT_NE(session, nullptr);


    StreamConfiguration config;
    Stream yuvStream;
    yuvStream.id = 0;
    yuvStream.format = HAL_PIXEL_FORMAT_YCbCr_420_888;
    yuvStream.width = 320;
    yuvStream.height = 240;
    yuvStream.streamType = StreamType::OUTPUT;
    config.streams.push_back(yuvStream);

    std::vector<HalStream> halStreams;
    st = session->configureStreams(config, &halStreams);
    ASSERT_TRUE(st.isOk());

    const int kNumFrames = 30;

    auto startTime = std::chrono::steady_clock::now();


    for (int i = 0; i < kNumFrames; i++) {
        CaptureRequest req;
        req.frameNumber = i;

        StreamBuffer buf;
        buf.streamId = 0;
        buf.bufferId = i;
        buf.buffer = nullptr;
        buf.status = BufferStatus::OK;
        req.outputBuffers.push_back(buf);

        std::vector<CaptureRequest> requests = {req};
        std::vector<BufferCache> caches;
        int32_t numProcessed = 0;
        session->processCaptureRequest(requests, caches, &numProcessed);
    }


    ASSERT_TRUE(cb->waitForResults(kNumFrames, 30000))
        << "Timed out waiting for " << kNumFrames << " frames";

    auto endTime = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    double avgLatencyMs = elapsedMs / kNumFrames;


    fprintf(stderr, "\n=== Performance Results ===\n");
    fprintf(stderr, "  Frames: %d\n", kNumFrames);
    fprintf(stderr, "  Total time: %.1f ms\n", elapsedMs);
    fprintf(stderr, "  Avg latency/frame: %.1f ms\n", avgLatencyMs);
    fprintf(stderr, "  Throughput: %.1f fps\n", kNumFrames / (elapsedMs / 1000.0));
    fprintf(stderr, "===========================\n\n");


    EXPECT_LT(avgLatencyMs, 100.0)
        << "Average per-frame latency " << avgLatencyMs
        << " ms exceeds 100ms threshold";

    session->close();
}


TEST_F(HALIntegrationTest, ConstructDefaultRequestSettings) {
    auto device = getDevice("0");
    auto [session, cb] = openSession(device);
    configureDefaultStreams(session);

    android::CameraMetadata meta;
    auto status = session->constructDefaultRequestSettings(
        RequestTemplate::PREVIEW, &meta);
    ASSERT_TRUE(status.isOk());


    EXPECT_TRUE(meta.exists(ANDROID_CONTROL_AE_MODE));

    EXPECT_TRUE(meta.exists(ANDROID_CONTROL_AF_MODE));

    EXPECT_TRUE(meta.exists(ANDROID_CONTROL_AWB_MODE));

    session->close();
}


TEST_F(HALIntegrationTest, GetConcurrentCameraIds) {
    std::vector<ConcurrentCameraIdCombination> combos;
    auto status = provider->getConcurrentCameraIds(&combos);
    ASSERT_TRUE(status.isOk());
    EXPECT_TRUE(combos.empty());
}


TEST_F(HALIntegrationTest, ConcurrentStreamCombinationNotSupported) {
    std::vector<CameraIdAndStreamCombination> configs;
    bool support = true;
    auto status = provider->isConcurrentStreamCombinationSupported(configs, &support);
    ASSERT_TRUE(status.isOk());
    EXPECT_FALSE(support);
}
