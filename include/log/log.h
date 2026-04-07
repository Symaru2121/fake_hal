#ifndef LOG_LOG_H
#define LOG_LOG_H

#include <stdio.h>

#define ALOGI(...) printf("I: " __VA_ARGS__)
#define ALOGE(...) fprintf(stderr, "E: " __VA_ARGS__)
#define ALOGW(...) printf("W: " __VA_ARGS__)
#define ALOGD(...) printf("D: " __VA_ARGS__)

#endif
