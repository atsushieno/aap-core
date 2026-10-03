#include "aap/core/realtime.h"
extern "C" __attribute__((visibility("default"))) bool aap_rt_scope_consumer() {
    return aap::RealtimeScope::isActive();
}
