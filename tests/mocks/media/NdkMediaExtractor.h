#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/types.h>


typedef int32_t media_status_t;
#define AMEDIA_OK 0


typedef struct AMediaExtractor AMediaExtractor;
typedef struct AMediaCodec AMediaCodec;
typedef struct AMediaFormat AMediaFormat;


#define AMEDIAFORMAT_KEY_MIME          "mime"
#define AMEDIAFORMAT_KEY_WIDTH         "width"
#define AMEDIAFORMAT_KEY_HEIGHT        "height"
#define AMEDIAFORMAT_KEY_FRAME_RATE    "frame-rate"
#define AMEDIAFORMAT_KEY_COLOR_FORMAT  "color-format"


#define AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM 0x00000004
#define AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED -2


#define AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC 2


struct AMediaCodecBufferInfo {
    int32_t offset;
    int32_t size;
    int64_t presentationTimeUs;
    uint32_t flags;
};


inline AMediaExtractor* AMediaExtractor_new() { return nullptr; }
inline void AMediaExtractor_delete(AMediaExtractor*) {}
inline media_status_t AMediaExtractor_setDataSourceFd(AMediaExtractor*, int, off_t, off_t) { return AMEDIA_OK; }
inline size_t AMediaExtractor_getTrackCount(AMediaExtractor*) { return 0; }
inline AMediaFormat* AMediaExtractor_getTrackFormat(AMediaExtractor*, size_t) { return nullptr; }
inline void AMediaExtractor_selectTrack(AMediaExtractor*, size_t) {}
inline ssize_t AMediaExtractor_readSampleData(AMediaExtractor*, uint8_t*, size_t) { return -1; }
inline int64_t AMediaExtractor_getSampleTime(AMediaExtractor*) { return 0; }
inline bool AMediaExtractor_advance(AMediaExtractor*) { return false; }
inline media_status_t AMediaExtractor_seekTo(AMediaExtractor*, int64_t, int) { return AMEDIA_OK; }

inline AMediaCodec* AMediaCodec_createDecoderByType(const char*) { return nullptr; }
inline media_status_t AMediaCodec_configure(AMediaCodec*, AMediaFormat*, void*, void*, uint32_t) { return AMEDIA_OK; }
inline media_status_t AMediaCodec_start(AMediaCodec*) { return AMEDIA_OK; }
inline void AMediaCodec_stop(AMediaCodec*) {}
inline void AMediaCodec_delete(AMediaCodec*) {}
inline media_status_t AMediaCodec_flush(AMediaCodec*) { return AMEDIA_OK; }
inline ssize_t AMediaCodec_dequeueInputBuffer(AMediaCodec*, int64_t) { return -1; }
inline uint8_t* AMediaCodec_getInputBuffer(AMediaCodec*, size_t, size_t*) { return nullptr; }
inline media_status_t AMediaCodec_queueInputBuffer(AMediaCodec*, size_t, off_t, size_t, uint64_t, uint32_t) { return AMEDIA_OK; }
inline ssize_t AMediaCodec_dequeueOutputBuffer(AMediaCodec*, AMediaCodecBufferInfo*, int64_t) { return -1; }
inline uint8_t* AMediaCodec_getOutputBuffer(AMediaCodec*, size_t, size_t*) { return nullptr; }
inline void AMediaCodec_releaseOutputBuffer(AMediaCodec*, size_t, bool) {}
inline AMediaFormat* AMediaCodec_getOutputFormat(AMediaCodec*) { return nullptr; }

inline AMediaFormat* AMediaFormat_new() { return nullptr; }
inline void AMediaFormat_delete(AMediaFormat*) {}
inline bool AMediaFormat_getString(AMediaFormat*, const char*, const char**) { return false; }
inline bool AMediaFormat_getInt32(AMediaFormat*, const char*, int32_t*) { return false; }
inline bool AMediaFormat_getFloat(AMediaFormat*, const char*, float*) { return false; }
