#include "FakeCameraProvider.h"
#include <android/log.h>

#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, "FakeHAL", __VA_ARGS__)

int main(int argc, char* argv[]) {
    const char* videoPath = "/data/local/tmp/fake_video.mp4";
    if (argc > 1) videoPath = argv[1];

    ALOGI("FakeHAL starting, video: %s", videoPath);
    fake_hal::FakeCameraProvider::instantiate(videoPath);
    return 0;
}
