#pragma once


#include <memory>

namespace android {

template <typename T>
class sp : public std::shared_ptr<T> {
public:
    using std::shared_ptr<T>::shared_ptr;

    sp() = default;
    sp(T* ptr) : std::shared_ptr<T>(ptr) {}
    sp(const std::shared_ptr<T>& other) : std::shared_ptr<T>(other) {}
};

}
