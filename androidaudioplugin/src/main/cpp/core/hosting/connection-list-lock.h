#pragma once
#include <mutex>

namespace aap::internal {
// Private coordination for the library's connection-list readers and JNI writers. Public
// headers and the connection-list layout stay unchanged. Never hold this across Binder IPC.
__attribute__((visibility("hidden"))) inline std::recursive_mutex& connectionListMutex() {
    static auto* mutex = new std::recursive_mutex();
    return *mutex;
}
}
