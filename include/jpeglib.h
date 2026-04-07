#ifndef JPEGLIB_H
#define JPEGLIB_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    int dummy;
} jpeg_error_mgr;

typedef struct {
    jpeg_error_mgr* err;
    int image_width;
    int image_height;
    int input_components;
    int output_components;
    int num_components;
    int jpeg_color_space;
    int out_color_space;
    int scale_num;
    int scale_denom;
    int dct_method;
    int progressive_mode;
    int data_precision;
    int comps_in_scan;
    int Ss, Se, Ah, Al;
    int restart_interval;
    int MCUs_per_row;
    int MCU_rows_in_scan;
    int blocks_in_MCU;
    int MCU_membership[10];
    int quant_tbl_ptrs[4];
    int dc_huff_tbl_ptrs[4];
    int ac_huff_tbl_ptrs[4];
    int arith_dc_L[16];
    int arith_dc_U[16];
    int arith_ac_K[16];
    int restart_in_rows;
    int write_JFIF_header;
    int write_Adobe_marker;
    int next_scanline;
    int max_h_samp_factor;
    int max_v_samp_factor;
    int total_iMCU_rows;
    int comp_info[4];
    int actual_number_of_tables;
    int quantval[4][64];
    int send_table;
    int scan_info;
    int num_scans;
    int optimize_coding;
    int smoothing_factor;
    int CCIR601_sampling;
    int JFIF_major_version;
    int JFIF_minor_version;
    int density_unit;
    int X_density;
    int Y_density;
} jpeg_compress_struct;

typedef struct {
    jpeg_error_mgr* err;
    FILE* input_file;
    int image_width;
    int image_height;
    int output_components;
    int out_color_space;
    int scale_num;
    int scale_denom;
    int buffered_image;
    int raw_data_out;
    int dct_method;
    int do_fancy_upsampling;
    int do_block_smoothing;
    int quantize_colors;
    int dither_mode;
    int two_pass_quantize;
    int desired_number_of_colors;
    int enable_1pass_quant;
    int enable_external_quant;
    int enable_2pass_quant;
    int output_gamma;
} jpeg_decompress_struct;

typedef void (*jpeg_error_exit)(jpeg_compress_struct*);

void jpeg_std_error(jpeg_error_mgr* err);
void jpeg_create_compress(jpeg_compress_struct* cinfo);
void jpeg_create_decompress(jpeg_decompress_struct* cinfo);
void jpeg_destroy_compress(jpeg_compress_struct* cinfo);
void jpeg_destroy_decompress(jpeg_decompress_struct* cinfo);
void jpeg_set_defaults(jpeg_compress_struct* cinfo);
void jpeg_set_quality(jpeg_compress_struct* cinfo, int quality, int force_baseline);
void jpeg_start_compress(jpeg_compress_struct* cinfo, int write_all_tables);
void jpeg_write_scanlines(jpeg_compress_struct* cinfo, uint8_t** scanlines, int num_lines);
void jpeg_finish_compress(jpeg_compress_struct* cinfo);
void jpeg_stdio_dest(jpeg_compress_struct* cinfo, FILE* outfile);
void jpeg_start_decompress(jpeg_decompress_struct* cinfo);
int jpeg_read_scanlines(jpeg_decompress_struct* cinfo, uint8_t** scanlines, int max_lines);
int jpeg_finish_decompress(jpeg_decompress_struct* cinfo);
int jpeg_read_header(jpeg_decompress_struct* cinfo, int require_image);

#endif
