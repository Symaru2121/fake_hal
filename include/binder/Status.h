#ifndef BINDER_STATUS_H
#define BINDER_STATUS_H

#include <string>

namespace android {
namespace binder {

class Status {
public:
    Status() : mException(0) {}
    Status(int32_t exception, const std::string& message)
        : mException(exception), mMessage(message) {}

    bool isOk() const { return mException == 0; }
    int32_t exceptionCode() const { return mException; }
    std::string exceptionMessage() const { return mMessage; }

    static Status ok() { return Status(); }

private:
    int32_t mException;
    std::string mMessage;
};

}
}

#endif
