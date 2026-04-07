#pragma once

#include <cstdint>
#include <vector>

namespace fake_hal {


class JpegEncoder {
public:
    struct ExifData {
        int    iso           = 100;
        float  exposureSec   = 1.0f / 60.0f;
        float  fNumber       = 1.85f;
        float  focalLength   = 6.81f;
        int    imageWidth    = 0;
        int    imageHeight   = 0;
        char   make[64]      = "Google";
        char   model[64]     = "Pixel 7";
        char   software[64]  = "Pixel Experience";
        bool   hasGps        = false;
        double gpsLat        = 0.0;
        double gpsLon        = 0.0;
        float  gpsAlt        = 0.0f;
    };

    JpegEncoder() = default;


    bool encode(
        const uint8_t* nv21,
        int width, int height,
        int quality,
        const ExifData& exif,
        std::vector<uint8_t>& outJpeg
    );

private:

    std::vector<uint8_t> buildExif(const ExifData& exif);


    static void injectExif(
        std::vector<uint8_t>& jpeg,
        const std::vector<uint8_t>& exifData
    );
};

}
