#pragma once

#ifdef __ANDROID__
#include <sys/system_properties.h>
#else

inline int __system_property_get(const char* name, char* value) {

    if (strcmp(name, "ro.serialno") == 0) {
        strcpy(value, "EMULATOR123");
        return strlen(value);
    }
    if (strcmp(name, "ro.hardware") == 0) {
        strcpy(value, "ranchu");
        return strlen(value);
    }
    if (strcmp(name, "ro.product.model") == 0) {
        strcpy(value, "Pixel 7");
        return strlen(value);
    }
    value[0] = '\0';
    return 0;
}
#endif
