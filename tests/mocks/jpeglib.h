#pragma once


#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>


typedef unsigned char JSAMPLE;
typedef JSAMPLE* JSAMPROW;
typedef JSAMPROW* JSAMPARRAY;
typedef int boolean;
typedef unsigned int JDIMENSION;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


typedef enum {
    JCS_UNKNOWN,
    JCS_GRAYSCALE,
    JCS_RGB,
    JCS_YCbCr,
    JCS_CMYK,
    JCS_YCCK,
    JCS_EXT_RGB,
    JCS_EXT_RGBX,
    JCS_EXT_BGR,
    JCS_EXT_BGRX,
    JCS_EXT_XBGR,
    JCS_EXT_XRGB,
    JCS_EXT_RGBA,
    JCS_EXT_BGRA,
    JCS_EXT_ABGR,
    JCS_EXT_ARGB,
    JCS_RGB565
} J_COLOR_SPACE;


typedef enum {
    JDCT_ISLOW,
    JDCT_IFAST,
    JDCT_FLOAT
} J_DCT_METHOD;


struct jpeg_compress_struct;
typedef struct jpeg_compress_struct* j_compress_ptr;
struct jpeg_common_struct;
typedef struct jpeg_common_struct* j_common_ptr;

struct jpeg_decompress_struct;
typedef struct jpeg_decompress_struct* j_decompress_ptr;


struct jpeg_error_mgr {
    void (*error_exit)(j_common_ptr);
    void (*emit_message)(j_common_ptr, int);
    void (*output_message)(j_common_ptr);
    void (*format_message)(j_common_ptr, char*);
    void (*reset_error_mgr)(j_common_ptr);
    int msg_code;
    int trace_level;
    long num_warnings;
};


struct jpeg_destination_mgr {
    uint8_t* next_output_byte;
    size_t free_in_buffer;
    void (*init_destination)(j_compress_ptr);
    boolean (*empty_output_buffer)(j_compress_ptr);
    void (*term_destination)(j_compress_ptr);
};


struct jpeg_common_struct {
    struct jpeg_error_mgr* err;
};


struct _fkhd_compress_state {
    int quality;
    std::vector<uint8_t> y_plane;
    JDIMENSION width;
    JDIMENSION height;
    int components;
};


struct jpeg_compress_struct {
    struct jpeg_error_mgr* err;
    struct jpeg_destination_mgr* dest;
    JDIMENSION image_width;
    JDIMENSION image_height;
    int input_components;
    J_COLOR_SPACE in_color_space;
    J_DCT_METHOD dct_method;
    JDIMENSION next_scanline;

    void* _internal;
    size_t _bytes_written;
    bool _started;
};


struct _fkhd_decompress_state {
    JDIMENSION width;
    JDIMENSION height;
    int components;
    int quality;
    std::vector<uint8_t> y_plane;
};


struct jpeg_decompress_struct {
    struct jpeg_error_mgr* err;
    JDIMENSION image_width;
    JDIMENSION image_height;
    J_COLOR_SPACE out_color_space;
    int output_width;
    int output_height;
    int output_components;
    int output_scanline;

    const uint8_t* _src;
    size_t _src_len;
    void* _internal;
};


#ifndef JPEG_HEADER_OK
#define JPEG_HEADER_OK 1
#endif


static inline void _fkhd_write(struct jpeg_compress_struct* cinfo,
                               const uint8_t* data, size_t len) {
    if (!cinfo || !cinfo->dest) return;
    size_t remaining = len;
    const uint8_t* p = data;
    while (remaining > 0) {
        if (cinfo->dest->free_in_buffer == 0) {
            if (cinfo->dest->empty_output_buffer)
                cinfo->dest->empty_output_buffer(cinfo);
            else
                return;
        }
        size_t n = std::min(remaining, cinfo->dest->free_in_buffer);
        memcpy(cinfo->dest->next_output_byte, p, n);
        cinfo->dest->next_output_byte += n;
        cinfo->dest->free_in_buffer -= n;
        cinfo->_bytes_written += n;
        p += n;
        remaining -= n;
    }
}


inline struct jpeg_error_mgr* jpeg_std_error(struct jpeg_error_mgr* err) {
    if (err) {
        err->error_exit = nullptr;
        err->emit_message = nullptr;
        err->output_message = nullptr;
        err->format_message = nullptr;
        err->reset_error_mgr = nullptr;
        err->msg_code = 0;
        err->trace_level = 0;
        err->num_warnings = 0;
    }
    return err;
}

inline void jpeg_create_compress(struct jpeg_compress_struct* cinfo) {
    if (cinfo) {
        cinfo->image_width = 0;
        cinfo->image_height = 0;
        cinfo->input_components = 0;
        cinfo->in_color_space = JCS_UNKNOWN;
        cinfo->dct_method = JDCT_ISLOW;
        cinfo->next_scanline = 0;
        cinfo->dest = nullptr;
        cinfo->_internal = new _fkhd_compress_state();
        cinfo->_bytes_written = 0;
        cinfo->_started = false;
    }
}

inline void jpeg_set_defaults(struct jpeg_compress_struct*) {}

inline void jpeg_set_quality(struct jpeg_compress_struct* cinfo,
                             int quality, boolean) {
    if (cinfo && cinfo->_internal) {
        auto* st = static_cast<_fkhd_compress_state*>(cinfo->_internal);
        st->quality = quality;
    }
}

inline void jpeg_start_compress(struct jpeg_compress_struct* cinfo, boolean) {
    if (!cinfo) return;
    cinfo->next_scanline = 0;
    cinfo->_bytes_written = 0;
    cinfo->_started = true;

    auto* st = static_cast<_fkhd_compress_state*>(cinfo->_internal);
    if (st) {
        st->width = cinfo->image_width;
        st->height = cinfo->image_height;
        st->components = cinfo->input_components;
        st->y_plane.clear();
        st->y_plane.reserve((size_t)cinfo->image_width * cinfo->image_height);
    }


    if (cinfo->dest && cinfo->dest->init_destination) {
        cinfo->dest->init_destination(cinfo);
    }


    uint8_t soi[2] = {0xFF, 0xD8};
    _fkhd_write(cinfo, soi, 2);


    uint8_t app0[2 + 2 + 14];
    app0[0] = 0xFF;
    app0[1] = 0xE0;

    app0[2] = 0x00;
    app0[3] = 16;

    app0[4] = 'F'; app0[5] = 'K'; app0[6] = 'H'; app0[7] = 'D';

    uint32_t w = cinfo->image_width;
    uint32_t h = cinfo->image_height;
    memcpy(&app0[8], &w, 4);
    memcpy(&app0[12], &h, 4);
    app0[16] = (uint8_t)cinfo->input_components;
    app0[17] = st ? (uint8_t)st->quality : 90;
    _fkhd_write(cinfo, app0, sizeof(app0));
}

inline JDIMENSION jpeg_write_scanlines(struct jpeg_compress_struct* cinfo,
                                       JSAMPARRAY scanlines,
                                       JDIMENSION num_lines) {
    if (!cinfo) return 0;

    auto* st = static_cast<_fkhd_compress_state*>(cinfo->_internal);

    for (JDIMENSION i = 0; i < num_lines; i++) {

        if (st && scanlines && scanlines[i]) {
            JSAMPROW row = scanlines[i];
            int comps = cinfo->input_components;
            for (JDIMENSION col = 0; col < cinfo->image_width; col++) {
                st->y_plane.push_back(row[col * comps]);
            }
        }
        cinfo->next_scanline++;
    }


    if (st && cinfo->dest) {
        int q = st->quality > 0 ? st->quality : 50;

        size_t bytes_per_line = std::max((size_t)1,
            (size_t)cinfo->image_width * (size_t)cinfo->input_components
            * (size_t)q / 600);
        size_t total = bytes_per_line * num_lines;

        uint8_t buf[256];
        memset(buf, 0xAB, sizeof(buf));
        while (total > 0) {
            size_t chunk = std::min(total, sizeof(buf));
            _fkhd_write(cinfo, buf, chunk);
            total -= chunk;
        }
    }

    return num_lines;
}

inline void jpeg_finish_compress(struct jpeg_compress_struct* cinfo) {
    if (!cinfo) return;


    auto* st = static_cast<_fkhd_compress_state*>(cinfo->_internal);
    if (st && !st->y_plane.empty() && cinfo->dest) {


        size_t sw = ((size_t)st->width + 1) / 2;
        size_t sh = ((size_t)st->height + 1) / 2;
        std::vector<uint8_t> sub(sw * sh);
        for (size_t r = 0; r < sh; r++) {
            for (size_t c = 0; c < sw; c++) {
                size_t srcR = r * 2;
                size_t srcC = c * 2;
                if (srcR < st->height && srcC < st->width)
                    sub[r * sw + c] = st->y_plane[srcR * st->width + srcC];
                else
                    sub[r * sw + c] = 128;
            }
        }


        size_t payload = 4 + sub.size();
        if (payload > 65533) payload = 65533;
        size_t markerLen = 2 + payload;

        uint8_t hdr[4] = {0xFF, 0xEC,
                          (uint8_t)((markerLen >> 8) & 0xFF),
                          (uint8_t)(markerLen & 0xFF)};
        _fkhd_write(cinfo, hdr, 4);
        uint8_t magic[4] = {'F', 'K', 'P', 'X'};
        _fkhd_write(cinfo, magic, 4);
        size_t dataLen = payload - 4;
        _fkhd_write(cinfo, sub.data(), dataLen);
    }


    uint8_t eoi[2] = {0xFF, 0xD9};
    _fkhd_write(cinfo, eoi, 2);


    if (cinfo->dest && cinfo->dest->term_destination) {
        cinfo->dest->term_destination(cinfo);
    }
}

inline void jpeg_destroy_compress(struct jpeg_compress_struct* cinfo) {
    if (cinfo && cinfo->_internal) {
        delete static_cast<_fkhd_compress_state*>(cinfo->_internal);
        cinfo->_internal = nullptr;
    }
}


inline void jpeg_create_decompress(struct jpeg_decompress_struct* dinfo) {
    if (dinfo) {
        dinfo->image_width = 0;
        dinfo->image_height = 0;
        dinfo->_src = nullptr;
        dinfo->_src_len = 0;
        dinfo->_internal = nullptr;
    }
}

inline void jpeg_mem_src(struct jpeg_decompress_struct* dinfo,
                         const unsigned char* inbuffer,
                         unsigned long insize) {
    if (dinfo) {
        dinfo->_src = inbuffer;
        dinfo->_src_len = (size_t)insize;
    }
}


inline int jpeg_read_header(struct jpeg_decompress_struct* dinfo,
                            boolean ) {
    if (!dinfo || !dinfo->_src || dinfo->_src_len < 4) return 0;

    const uint8_t* buf = dinfo->_src;
    size_t len = dinfo->_src_len;

    JDIMENSION w = 0, h = 0;
    int comps = 3, quality = 90;
    bool foundFKHD = false;
    std::vector<uint8_t> y_sub;


    size_t pos = 0;
    while (pos + 1 < len) {
        if (buf[pos] != 0xFF) { pos++; continue; }
        uint8_t marker = buf[pos + 1];
        pos += 2;


        if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7)) {
            continue;
        }

        if (marker == 0x01 || marker == 0x00) continue;


        if (pos + 2 > len) break;
        uint16_t segLen = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
        if (segLen < 2) break;
        size_t dataStart = pos + 2;
        size_t payloadLen = segLen - 2;
        size_t segEnd = pos + segLen;
        if (segEnd > len) break;


        if (marker == 0xE0 && payloadLen >= 14) {
            if (buf[dataStart] == 'F' && buf[dataStart+1] == 'K' &&
                buf[dataStart+2] == 'H' && buf[dataStart+3] == 'D') {
                memcpy(&w, &buf[dataStart + 4], 4);
                memcpy(&h, &buf[dataStart + 8], 4);
                comps = buf[dataStart + 12];
                quality = buf[dataStart + 13];
                foundFKHD = true;
            }
        }


        if (marker == 0xEC && payloadLen >= 4) {
            if (buf[dataStart] == 'F' && buf[dataStart+1] == 'K' &&
                buf[dataStart+2] == 'P' && buf[dataStart+3] == 'X') {
                size_t pixLen = payloadLen - 4;
                y_sub.assign(&buf[dataStart + 4], &buf[dataStart + 4 + pixLen]);
            }
        }

        pos = segEnd;
    }

    if (foundFKHD && w > 0 && h > 0) {
        dinfo->image_width = w;
        dinfo->image_height = h;
        dinfo->output_width = (int)w;
        dinfo->output_height = (int)h;
        dinfo->output_components = comps > 0 ? comps : 3;
        dinfo->out_color_space = JCS_RGB;


        auto* st = new _fkhd_decompress_state();
        st->width = w;
        st->height = h;
        st->components = dinfo->output_components;
        st->quality = quality;
        st->y_plane = std::move(y_sub);
        dinfo->_internal = st;
    } else {

        dinfo->image_width = 1;
        dinfo->image_height = 1;
        dinfo->output_width = 1;
        dinfo->output_height = 1;
        dinfo->output_components = 3;
        dinfo->out_color_space = JCS_RGB;
    }

    return JPEG_HEADER_OK;
}

inline boolean jpeg_start_decompress(struct jpeg_decompress_struct* dinfo) {
    if (!dinfo) return FALSE;
    dinfo->output_scanline = 0;

    if (dinfo->out_color_space == JCS_YCbCr || dinfo->out_color_space == JCS_RGB) {
        dinfo->output_components = 3;
    } else if (dinfo->out_color_space == JCS_GRAYSCALE) {
        dinfo->output_components = 1;
    }
    return TRUE;
}

inline JDIMENSION jpeg_read_scanlines(struct jpeg_decompress_struct* dinfo,
                                      JSAMPARRAY scanlines,
                                      JDIMENSION max_lines) {
    if (!dinfo || !scanlines || max_lines == 0) return 0;
    JDIMENSION lines_to_read = max_lines;
    if ((JDIMENSION)(dinfo->output_scanline + max_lines) > (JDIMENSION)dinfo->output_height) {
        lines_to_read = (JDIMENSION)dinfo->output_height - (JDIMENSION)dinfo->output_scanline;
    }

    auto* st = static_cast<_fkhd_decompress_state*>(dinfo->_internal);

    int comps = dinfo->output_components;
    JDIMENSION outW = (JDIMENSION)dinfo->output_width;
    size_t row_stride = (size_t)outW * (size_t)comps;


    size_t sw = st ? ((size_t)st->width + 1) / 2 : 0;

    for (JDIMENSION i = 0; i < lines_to_read; ++i) {
        uint8_t* row = scanlines[i];
        if (!row) {
            dinfo->output_scanline++;
            continue;
        }

        JDIMENSION srcRow = (JDIMENSION)dinfo->output_scanline;

        if (st && !st->y_plane.empty()) {

            size_t subR = (size_t)srcRow / 2;
            for (JDIMENSION col = 0; col < outW; col++) {
                size_t subC = (size_t)col / 2;
                uint8_t Y = 128;
                if (subR < ((size_t)st->height + 1) / 2 && subC < sw) {
                    size_t idx = subR * sw + subC;
                    if (idx < st->y_plane.size())
                        Y = st->y_plane[idx];
                }
                if (comps == 1) {
                    row[col] = Y;
                } else {

                    row[col * comps + 0] = Y;
                    row[col * comps + 1] = 128;
                    row[col * comps + 2] = 128;
                }
            }
        } else {

            memset(row, 0x80, row_stride);
        }

        dinfo->output_scanline++;
    }
    return lines_to_read;
}

inline void jpeg_finish_decompress(struct jpeg_decompress_struct* dinfo) {
    (void)dinfo;
}

inline void jpeg_destroy_decompress(struct jpeg_decompress_struct* dinfo) {
    if (dinfo && dinfo->_internal) {
        delete static_cast<_fkhd_decompress_state*>(dinfo->_internal);
        dinfo->_internal = nullptr;
    }
}
