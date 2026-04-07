#pragma once

#include <cstdint>
#include <cutils/native_handle.h>


#define HAL_PIXEL_FORMAT_BLOB                 0x21
#define HAL_PIXEL_FORMAT_YCbCr_420_888        0x23
#define HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED 0x22


#define GRALLOC_USAGE_SW_WRITE_OFTEN          0x00000030
#define GRALLOC1_PRODUCER_USAGE_CAMERA        0x00020000
#define GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN 0x00000030


#define CAMERA_BLOB_ID_JPEG 0x00FF

struct CameraBlob {
    uint32_t blobId;
    uint32_t blobSize;
};
