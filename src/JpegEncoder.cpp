#include "JpegEncoder.h"

#include <cstdio>
#include <jpeglib.h>
#include <jerror.h>

#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <cmath>

#define LOG_TAG "FakeHAL_Jpeg"
#include <log/log.h>

namespace fake_hal {


static void writeU16LE(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(x & 0xFF);
    v.push_back((x >> 8) & 0xFF);
}
static void writeU32LE(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((x >>  0) & 0xFF);
    v.push_back((x >>  8) & 0xFF);
    v.push_back((x >> 16) & 0xFF);
    v.push_back((x >> 24) & 0xFF);
}
static void writeRationalLE(std::vector<uint8_t>& v, uint32_t num, uint32_t den) {
    writeU32LE(v, num);
    writeU32LE(v, den);
}


static void writeIFDEntry(std::vector<uint8_t>& v,
                          uint16_t tag, uint16_t type,
                          uint32_t count, uint32_t valueOrOffset) {
    writeU16LE(v, tag);
    writeU16LE(v, type);
    writeU32LE(v, count);
    writeU32LE(v, valueOrOffset);
}

std::vector<uint8_t> JpegEncoder::buildExif(const ExifData& exif) {


    std::vector<uint8_t> app1;


    const char exifMagic[] = "Exif\0\0";
    for (int i = 0; i < 6; i++) app1.push_back((uint8_t)exifMagic[i]);

    size_t tiffStart = app1.size();
    (void)tiffStart;


    app1.push_back('I'); app1.push_back('I');
    writeU16LE(app1, 42);
    writeU32LE(app1, 8);


    std::string makeStr(exif.make);    makeStr += '\0';
    std::string modelStr(exif.model);  modelStr += '\0';
    std::string softStr(exif.software); softStr += '\0';


    uint16_t ifd0Count = 5;


    size_t ifd0Size = 2 + 12 * ifd0Count + 4;
    size_t ifd0End  = 8 + ifd0Size;


    size_t makeOffset  = ifd0End;
    size_t modelOffset = makeOffset  + makeStr.size();
    size_t softOffset  = modelOffset + modelStr.size();
    size_t exifSubIFDOffset = softOffset + softStr.size();


    uint16_t exifCount = 4;
    size_t exifSubIFDSize = 2 + 12 * exifCount + 4;
    size_t exifDataOffset = exifSubIFDOffset + exifSubIFDSize;


    uint32_t expNum = 1;
    uint32_t expDen = (uint32_t)(1.0f / exif.exposureSec + 0.5f);
    if (expDen == 0) expDen = 60;

    uint32_t fnumNum = (uint32_t)(exif.fNumber * 100);
    uint32_t fnumDen = 100;

    uint32_t flNum = (uint32_t)(exif.focalLength * 100);
    uint32_t flDen = 100;

    size_t expTimeOff = exifDataOffset;
    size_t fNumOff    = expTimeOff + 8;
    size_t flOff      = fNumOff   + 8;


    writeU16LE(app1, ifd0Count);


    writeIFDEntry(app1, 0x010F, 2, (uint32_t)makeStr.size(), (uint32_t)makeOffset);

    writeIFDEntry(app1, 0x0110, 2, (uint32_t)modelStr.size(), (uint32_t)modelOffset);

    writeIFDEntry(app1, 0x0131, 2, (uint32_t)softStr.size(), (uint32_t)softOffset);

    writeIFDEntry(app1, 0x0112, 3, 1, 1);

    writeIFDEntry(app1, 0x8769, 4, 1, (uint32_t)exifSubIFDOffset);

    writeU32LE(app1, 0);


    for (char c : makeStr)  app1.push_back((uint8_t)c);
    for (char c : modelStr) app1.push_back((uint8_t)c);
    for (char c : softStr)  app1.push_back((uint8_t)c);


    writeU16LE(app1, exifCount);


    writeIFDEntry(app1, 0x829A, 5, 1, (uint32_t)expTimeOff);

    writeIFDEntry(app1, 0x829D, 5, 1, (uint32_t)fNumOff);

    writeIFDEntry(app1, 0x8827, 3, 1, (uint32_t)exif.iso);

    writeIFDEntry(app1, 0x920A, 5, 1, (uint32_t)flOff);

    writeU32LE(app1, 0);


    writeRationalLE(app1, expNum, expDen);
    writeRationalLE(app1, fnumNum, fnumDen);
    writeRationalLE(app1, flNum, flDen);

    return app1;
}

void JpegEncoder::injectExif(std::vector<uint8_t>& jpeg,
                              const std::vector<uint8_t>& exifData)
{


    if (jpeg.size() < 2) return;


    uint16_t app1Len = (uint16_t)(2 + exifData.size());

    std::vector<uint8_t> app1Marker;
    app1Marker.push_back(0xFF);
    app1Marker.push_back(0xE1);
    app1Marker.push_back((app1Len >> 8) & 0xFF);
    app1Marker.push_back(app1Len & 0xFF);
    for (uint8_t b : exifData) app1Marker.push_back(b);


    jpeg.insert(jpeg.begin() + 2, app1Marker.begin(), app1Marker.end());
}


struct jpeg_dest_mgr_impl {
    struct jpeg_destination_mgr mgr;
    std::vector<uint8_t>* buffer;
};

static void init_destination(j_compress_ptr cinfo) {
    auto* dest = reinterpret_cast<jpeg_dest_mgr_impl*>(cinfo->dest);
    dest->buffer->resize(65536);
    cinfo->dest->next_output_byte  = dest->buffer->data();
    cinfo->dest->free_in_buffer    = dest->buffer->size();
}

static boolean empty_output_buffer(j_compress_ptr cinfo) {
    auto* dest = reinterpret_cast<jpeg_dest_mgr_impl*>(cinfo->dest);
    size_t oldSize = dest->buffer->size();
    dest->buffer->resize(oldSize * 2);
    cinfo->dest->next_output_byte = dest->buffer->data() + oldSize;
    cinfo->dest->free_in_buffer   = oldSize;
    return TRUE;
}

static void term_destination(j_compress_ptr cinfo) {
    auto* dest = reinterpret_cast<jpeg_dest_mgr_impl*>(cinfo->dest);
    size_t finalSize = dest->buffer->size() - cinfo->dest->free_in_buffer;
    dest->buffer->resize(finalSize);
}

bool JpegEncoder::encode(const uint8_t* nv21, int width, int height,
                         int quality, const ExifData& exif,
                         std::vector<uint8_t>& outJpeg)
{
#ifdef FAKE_HAL_TEST_BUILD
    (void)nv21;
    (void)quality;

    outJpeg.clear();
    outJpeg.reserve(512);
    outJpeg.push_back(0xFF);
    outJpeg.push_back(0xD8);


    for (int i = 0; i < 256; i++) outJpeg.push_back((uint8_t)0xAB);

    outJpeg.push_back(0xFF);
    outJpeg.push_back(0xD9);


    ExifData exif2 = exif;
    exif2.imageWidth = width;
    exif2.imageHeight = height;
    auto exifDataLocal = buildExif(exif2);
    injectExif(outJpeg, exifDataLocal);

    ALOGD("JpegEncoder: encoded %dx%d -> %zu bytes (q=%d)",
          width, height, outJpeg.size(), quality);
    return true;
#endif

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);


    jpeg_dest_mgr_impl destImpl;
    destImpl.buffer = &outJpeg;
    destImpl.mgr.init_destination    = init_destination;
    destImpl.mgr.empty_output_buffer = empty_output_buffer;
    destImpl.mgr.term_destination    = term_destination;
    cinfo.dest = &destImpl.mgr;

    cinfo.image_width      = width;
    cinfo.image_height     = height;
    cinfo.input_components = 3;
    cinfo.in_color_space   = JCS_YCbCr;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    cinfo.dct_method = JDCT_ISLOW;

    jpeg_start_compress(&cinfo, TRUE);


    std::vector<uint8_t> rowBuf(width * 3);
    JSAMPROW rowPointer[1] = { rowBuf.data() };

    const uint8_t* yPlane  = nv21;
    const uint8_t* uvPlane = nv21 + width * height;

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int uvIdx = (row / 2) * (width / 2) + (col / 2);

            uint8_t Y  = yPlane[row * width + col];
            uint8_t V  = uvPlane[uvIdx * 2 + 0];
            uint8_t U  = uvPlane[uvIdx * 2 + 1];


            rowBuf[col * 3 + 0] = Y;
            rowBuf[col * 3 + 1] = U;
            rowBuf[col * 3 + 2] = V;
        }
        jpeg_write_scanlines(&cinfo, rowPointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);


    auto exifData = buildExif(exif);
    injectExif(outJpeg, exifData);

    ALOGD("JpegEncoder: encoded %dx%d -> %zu bytes (q=%d)",
          width, height, outJpeg.size(), quality);
    return true;
}

}
