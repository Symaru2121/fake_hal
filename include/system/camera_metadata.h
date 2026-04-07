#ifndef SYSTEM_CAMERA_METADATA_H
#define SYSTEM_CAMERA_METADATA_H

#include <stdint.h>

typedef struct camera_metadata camera_metadata_t;

camera_metadata_t* allocate_camera_metadata(size_t entry_capacity, size_t data_capacity);
void free_camera_metadata(camera_metadata_t* metadata);
int add_camera_metadata_entry(camera_metadata_t* metadata, uint32_t tag, const void* data, size_t data_count);
int get_camera_metadata_entry(camera_metadata_t* metadata, uint32_t tag, const void** data, size_t* data_count);

#endif
