#pragma once

#include <cstdint>

struct AIBinder;

typedef int32_t binder_status_t;
#ifndef STATUS_OK
#define STATUS_OK 0
#endif

inline binder_status_t AServiceManager_addService(AIBinder*, const char*) {
    return STATUS_OK;
}

inline AIBinder* AServiceManager_checkService(const char*) {
    return nullptr;
}

inline AIBinder* AServiceManager_getService(const char*) {
    return nullptr;
}
