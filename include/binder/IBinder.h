#ifndef BINDER_IBINDER_H
#define BINDER_IBINDER_H

#include <stdint.h>
#include <string>

namespace android {

class RefBase {
public:
    RefBase() {}
    virtual ~RefBase() {}
};

template<typename T>
class sp {
public:
    sp() : mPtr(nullptr) {}
    sp(T* ptr) : mPtr(ptr) {}
    ~sp() { if (mPtr) mPtr->decStrong(this); }

    T* get() const { return mPtr; }
    T* operator->() const { return mPtr; }

private:
    T* mPtr;
};

namespace binder {

class IBinder : public RefBase {
public:
    virtual ~IBinder() {}

    virtual std::string getInterfaceDescriptor() const = 0;
};

}
}

#endif
