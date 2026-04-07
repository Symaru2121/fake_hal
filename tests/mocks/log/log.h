#pragma once

#include <cstdio>

#define LOG_TAG_DEFINED

#ifndef LOG_TAG
#define LOG_TAG "FakeHAL"
#endif

#define ALOGI(fmt, ...) fprintf(stderr, "I/%s: " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define ALOGD(fmt, ...) fprintf(stderr, "D/%s: " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define ALOGW(fmt, ...) fprintf(stderr, "W/%s: " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define ALOGE(fmt, ...) fprintf(stderr, "E/%s: " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define ALOGV(fmt, ...) fprintf(stderr, "V/%s: " fmt "\n", LOG_TAG, ##__VA_ARGS__)
