#include "VideoFrameReader.h"

#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaError.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#define LOG_TAG "FakeHAL_VideoReader"
#include <log/log.h>


#define COLOR_FormatYUV420Planar      19
#define COLOR_FormatYUV420SemiPlanar  21
#define COLOR_FormatYUV420PackedSemiPlanar 39

namespace fake_hal {

VideoFrameReader::VideoFrameReader(const std::string& filePath)
    : filePath_(filePath) {}

VideoFrameReader::~VideoFrameReader() {
    close();
}

bool VideoFrameReader::open(int targetWidth, int targetHeight) {
    targetWidth_  = targetWidth;
    targetHeight_ = targetHeight;


    int fd = ::open(filePath_.c_str(), O_RDONLY);
    if (fd < 0) {
        ALOGE("VideoFrameReader: cannot open %s: %s",
              filePath_.c_str(), strerror(errno));
        return false;
    }

    struct stat st;
    fstat(fd, &st);
    off64_t fileSize = st.st_size;

    extractor_ = AMediaExtractor_new();
    media_status_t status = AMediaExtractor_setDataSourceFd(
        extractor_, fd, 0, fileSize);
    ::close(fd);

    if (status != AMEDIA_OK) {
        ALOGE("VideoFrameReader: setDataSourceFd failed: %d", status);
        AMediaExtractor_delete(extractor_);
        extractor_ = nullptr;
        return false;
    }

    if (!findVideoTrack()) {
        ALOGE("VideoFrameReader: no video track found in %s", filePath_.c_str());
        return false;
    }

    AMediaExtractor_selectTrack(extractor_, videoTrackIndex_);


    const char* mime = nullptr;
    AMediaFormat_getString(format_, AMEDIAFORMAT_KEY_MIME, &mime);

    codec_ = AMediaCodec_createDecoderByType(mime);
    if (!codec_) {
        ALOGE("VideoFrameReader: cannot create decoder for %s", mime);
        return false;
    }

    status = AMediaCodec_configure(codec_, format_, nullptr, nullptr, 0);
    if (status != AMEDIA_OK) {
        ALOGE("VideoFrameReader: codec configure failed: %d", status);
        return false;
    }

    status = AMediaCodec_start(codec_);
    if (status != AMEDIA_OK) {
        ALOGE("VideoFrameReader: codec start failed: %d", status);
        return false;
    }


    size_t nv21Size = (size_t)(width_ * height_ * 3 / 2);
    lastFrameBuf_.resize(nv21Size, 128);
    convBuf_.resize(nv21Size * 2);

    isOpen_ = true;
    ALOGI("VideoFrameReader: opened %s (%dx%d @ %d fps)",
          filePath_.c_str(), width_, height_, fps_);
    return true;
}

bool VideoFrameReader::findVideoTrack() {
    size_t numTracks = AMediaExtractor_getTrackCount(extractor_);
    for (size_t i = 0; i < numTracks; i++) {
        AMediaFormat* fmt = AMediaExtractor_getTrackFormat(extractor_, i);
        const char* mime = nullptr;
        AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &mime);
        if (mime && strncmp(mime, "video/", 6) == 0) {
            videoTrackIndex_ = (int)i;
            format_ = fmt;


            int32_t w = 0, h = 0;
            AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, &w);
            AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, &h);

            if (targetWidth_ > 0 && targetHeight_ > 0) {
                width_  = targetWidth_;
                height_ = targetHeight_;
            } else {
                width_  = w;
                height_ = h;
            }


            float fpsF = 30.0f;
            AMediaFormat_getFloat(fmt, AMEDIAFORMAT_KEY_FRAME_RATE, &fpsF);
            fps_ = (int)fpsF;

            ALOGI("VideoFrameReader: found video track %zu: %s %dx%d @ %.1f fps",
                  i, mime, w, h, fpsF);
            return true;
        }
        AMediaFormat_delete(fmt);
    }
    return false;
}

void VideoFrameReader::close() {
    isOpen_ = false;
    if (codec_) {
        AMediaCodec_stop(codec_);
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
    }
    if (extractor_) {
        AMediaExtractor_delete(extractor_);
        extractor_ = nullptr;
    }
    if (format_) {


        format_ = nullptr;
    }
}


void VideoFrameReader::i420ToNV21(const uint8_t* src, uint8_t* dst, int w, int h) {
    int ySize  = w * h;
    int uvSize = w * h / 4;

    const uint8_t* Y = src;
    const uint8_t* U = src + ySize;
    const uint8_t* V = src + ySize + uvSize;


    memcpy(dst, Y, ySize);


    uint8_t* dstUV = dst + ySize;
    for (int i = 0; i < uvSize; i++) {
        dstUV[i * 2 + 0] = V[i];
        dstUV[i * 2 + 1] = U[i];
    }
}


void VideoFrameReader::nv12ToNV21(const uint8_t* src, uint8_t* dst, int w, int h) {
    int ySize = w * h;
    memcpy(dst, src, ySize);

    const uint8_t* srcUV = src + ySize;
    uint8_t*       dstUV = dst + ySize;
    int uvSize = ySize / 2;

    for (int i = 0; i < uvSize; i += 2) {
        dstUV[i + 0] = srcUV[i + 1];
        dstUV[i + 1] = srcUV[i + 0];
    }
}


void VideoFrameReader::resizeNV21(const uint8_t* src, int srcW, int srcH,
                                   uint8_t* dst, int dstW, int dstH)
{

    float scaleX = (float)srcW / dstW;
    float scaleY = (float)srcH / dstH;

    for (int y = 0; y < dstH; y++) {
        float srcYF = y * scaleY;
        int   y0    = (int)srcYF;
        int   y1    = std::min(y0 + 1, srcH - 1);
        float fy    = srcYF - y0;

        for (int x = 0; x < dstW; x++) {
            float srcXF = x * scaleX;
            int   x0    = (int)srcXF;
            int   x1    = std::min(x0 + 1, srcW - 1);
            float fx    = srcXF - x0;

            float v = src[y0*srcW + x0] * (1-fx)*(1-fy)
                    + src[y0*srcW + x1] * fx    *(1-fy)
                    + src[y1*srcW + x0] * (1-fx)* fy
                    + src[y1*srcW + x1] * fx    * fy;

            dst[y*dstW + x] = (uint8_t)std::clamp((int)v, 0, 255);
        }
    }


    int srcUVW = srcW / 2, srcUVH = srcH / 2;
    int dstUVW = dstW / 2, dstUVH = dstH / 2;

    const uint8_t* srcUV = src + srcW * srcH;
    uint8_t*       dstUV = dst + dstW * dstH;

    float scaleUVX = (float)srcUVW / dstUVW;
    float scaleUVY = (float)srcUVH / dstUVH;

    for (int y = 0; y < dstUVH; y++) {
        float srcYF = y * scaleUVY;
        int   y0    = (int)srcYF;
        int   y1    = std::min(y0 + 1, srcUVH - 1);
        float fy    = srcYF - y0;

        for (int x = 0; x < dstUVW; x++) {
            float srcXF = x * scaleUVX;
            int   x0    = (int)srcXF;
            int   x1    = std::min(x0 + 1, srcUVW - 1);
            float fx    = srcXF - x0;

            for (int c = 0; c < 2; c++) {
                float v = srcUV[(y0*srcUVW + x0)*2 + c] * (1-fx)*(1-fy)
                        + srcUV[(y0*srcUVW + x1)*2 + c] * fx    *(1-fy)
                        + srcUV[(y1*srcUVW + x0)*2 + c] * (1-fx)* fy
                        + srcUV[(y1*srcUVW + x1)*2 + c] * fx    * fy;

                dstUV[(y*dstUVW + x)*2 + c] = (uint8_t)std::clamp((int)v, 0, 255);
            }
        }
    }
}

bool VideoFrameReader::nextFrame(uint8_t* outBuffer) {
    if (!isOpen_ || !codec_ || !extractor_) return false;

    static constexpr int64_t TIMEOUT_US = 50'000;
    using namespace std::chrono;

    bool gotFrame = false;
    int  retries  = 0;

    while (!gotFrame && retries < 10) {

        ssize_t inBufIdx = AMediaCodec_dequeueInputBuffer(codec_, TIMEOUT_US);
        if (inBufIdx >= 0) {
            size_t bufSize = 0;
            uint8_t* buf = AMediaCodec_getInputBuffer(codec_, inBufIdx, &bufSize);

            ssize_t sampleSize = AMediaExtractor_readSampleData(extractor_, buf, bufSize);
            int64_t pts = AMediaExtractor_getSampleTime(extractor_);

            if (sampleSize < 0) {

                ALOGI("VideoFrameReader: end of file, looping");
                AMediaCodec_queueInputBuffer(codec_, inBufIdx, 0, 0, 0,
                                             AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                AMediaExtractor_seekTo(extractor_, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);
                AMediaCodec_flush(codec_);
                AMediaCodec_start(codec_);
                retries++;
                continue;
            }

            AMediaCodec_queueInputBuffer(codec_, inBufIdx, 0, sampleSize, pts, 0);
            AMediaExtractor_advance(extractor_);
        }


        AMediaCodecBufferInfo info;
        ssize_t outBufIdx = AMediaCodec_dequeueOutputBuffer(codec_, &info, TIMEOUT_US);

        if (outBufIdx >= 0) {
            size_t outSize = 0;
            uint8_t* outBuf = AMediaCodec_getOutputBuffer(codec_, outBufIdx, &outSize);

            if (outBuf && !(info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)) {

                AMediaFormat* outFmt = AMediaCodec_getOutputFormat(codec_);
                int32_t colorFmt = COLOR_FormatYUV420SemiPlanar;
                AMediaFormat_getInt32(outFmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, &colorFmt);
                int32_t decW = width_, decH = height_;
                AMediaFormat_getInt32(outFmt, AMEDIAFORMAT_KEY_WIDTH, &decW);
                AMediaFormat_getInt32(outFmt, AMEDIAFORMAT_KEY_HEIGHT, &decH);
                AMediaFormat_delete(outFmt);


                uint8_t* nv21Src = convBuf_.data();
                if (colorFmt == COLOR_FormatYUV420Planar) {
                    i420ToNV21(outBuf, nv21Src, decW, decH);
                } else {

                    nv12ToNV21(outBuf, nv21Src, decW, decH);
                }


                if (decW != width_ || decH != height_) {
                    resizeNV21(nv21Src, decW, decH, outBuffer, width_, height_);
                } else {
                    memcpy(outBuffer, nv21Src, width_ * height_ * 3 / 2);
                }


                {
                    std::lock_guard<std::mutex> lk(lastFrameMutex_);
                    memcpy(lastFrameBuf_.data(), outBuffer, width_ * height_ * 3 / 2);
                }

                gotFrame = true;
            }

            AMediaCodec_releaseOutputBuffer(codec_, outBufIdx, false);

        } else if (outBufIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            ALOGI("VideoFrameReader: output format changed");
        }

        retries++;
    }

    if (!gotFrame) {

        return lastFrame(outBuffer);
    }

    return gotFrame;
}

bool VideoFrameReader::lastFrame(uint8_t* outBuffer) {
    std::lock_guard<std::mutex> lk(lastFrameMutex_);
    if (lastFrameBuf_.empty()) return false;
    memcpy(outBuffer, lastFrameBuf_.data(), lastFrameBuf_.size());
    return true;
}

}
