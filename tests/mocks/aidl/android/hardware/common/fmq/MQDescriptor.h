#pragma once


namespace aidl {
namespace android {
namespace hardware {
namespace common {
namespace fmq {

struct SynchronizedReadWrite {};

template <typename T, typename Flavor>
struct MQDescriptor {

};

}
}
}
}
}


namespace android {
namespace hardware {

struct kSynchronizedReadWrite {};

}
}


using aidl::android::hardware::common::fmq::MQDescriptor;
