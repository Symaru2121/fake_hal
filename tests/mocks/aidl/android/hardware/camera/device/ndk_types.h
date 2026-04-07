#pragma once


#include <cstdint>
#include <memory>
#include <string>


typedef int32_t binder_status_t;
#define STATUS_OK 0


#define EX_NONE 0
#define EX_UNSUPPORTED_OPERATION 12


struct AIBinder {
    int dummy = 0;
};


namespace ndk {

class SpAIBinder {
public:
    SpAIBinder() = default;
    explicit SpAIBinder(AIBinder* b) : binder_(b) {}
    AIBinder* get() const { return binder_; }
private:
    AIBinder* binder_ = nullptr;
};


class ScopedAStatus {
public:
    ScopedAStatus() : status_(STATUS_OK), exceptionCode_(EX_NONE) {}

    bool isOk() const { return status_ == STATUS_OK && exceptionCode_ == EX_NONE; }

    int32_t getServiceSpecificError() const { return serviceSpecificError_; }
    int32_t getExceptionCode() const { return exceptionCode_; }

    static ScopedAStatus ok() {
        return ScopedAStatus();
    }

    static ScopedAStatus fromServiceSpecificError(int32_t err) {
        ScopedAStatus s;
        s.status_ = err;
        s.serviceSpecificError_ = err;
        return s;
    }

    static ScopedAStatus fromExceptionCode(int32_t code) {
        ScopedAStatus s;
        s.exceptionCode_ = code;
        return s;
    }

private:
    binder_status_t status_ = STATUS_OK;
    int32_t serviceSpecificError_ = 0;
    int32_t exceptionCode_ = EX_NONE;
};


class ScopedFileDescriptor {
public:
    ScopedFileDescriptor() : fd_(-1) {}
    explicit ScopedFileDescriptor(int fd) : fd_(fd) {}
    int get() const { return fd_; }
private:
    int fd_;
};


class SharedRefBase : public std::enable_shared_from_this<SharedRefBase> {
public:
    virtual ~SharedRefBase() = default;

    template <typename T, typename... Args>
    static std::shared_ptr<T> make(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    SpAIBinder asBinder() {
        static AIBinder dummy;
        return SpAIBinder(&dummy);
    }
};

}
