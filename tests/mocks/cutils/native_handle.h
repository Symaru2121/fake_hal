#pragma once


#include <cstdint>
#include <cstdlib>
#include <cstring>

typedef struct native_handle {
    int version;
    int numFds;
    int numInts;
    int data[0];
} native_handle_t;


#ifndef BUFFER_HANDLE_T_DEFINED
#define BUFFER_HANDLE_T_DEFINED
typedef const native_handle_t* buffer_handle_t;
#endif


inline native_handle_t* native_handle_create(int numFds, int numInts) {
    if (numFds < 0 || numInts < 0) return nullptr;
    size_t mallocSize = sizeof(native_handle_t) + sizeof(int) * (size_t)(numFds + numInts);
    native_handle_t* h = (native_handle_t*)calloc(1, mallocSize);
    if (!h) return nullptr;
    h->version = (int)sizeof(native_handle_t);
    h->numFds  = numFds;
    h->numInts = numInts;
    return h;
}


inline void native_handle_delete(native_handle_t* h) {
    free(h);
}
