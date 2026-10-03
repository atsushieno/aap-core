#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>

// Loaded into the native test binary with DYLD_INSERT_LIBRARIES on macOS.
// An interpose section in the main executable is ignored by current dyld.
extern "C" bool aap_test_is_processing();
static void check() {
    if (aap_test_is_processing()) {
        // stdio/abort may acquire another platform lock. Fail without reentering the hook.
        const char message[] = "FAIL: pthread lock reached during process()\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        __builtin_trap();
    }
}
int guardedMutexLock(pthread_mutex_t* mutex) { check(); return pthread_mutex_lock(mutex); }
int guardedMutexTryLock(pthread_mutex_t* mutex) { check(); return pthread_mutex_trylock(mutex); }
int guardedReadLock(pthread_rwlock_t* lock) { check(); return pthread_rwlock_rdlock(lock); }
int guardedWriteLock(pthread_rwlock_t* lock) { check(); return pthread_rwlock_wrlock(lock); }
struct Interposition { const void* replacement; const void* original; };
__attribute__((used, section("__DATA,__interpose"))) static const Interposition interpositions[]{
    {reinterpret_cast<const void*>(guardedMutexLock), reinterpret_cast<const void*>(pthread_mutex_lock)},
    {reinterpret_cast<const void*>(guardedMutexTryLock), reinterpret_cast<const void*>(pthread_mutex_trylock)},
    {reinterpret_cast<const void*>(guardedReadLock), reinterpret_cast<const void*>(pthread_rwlock_rdlock)},
    {reinterpret_cast<const void*>(guardedWriteLock), reinterpret_cast<const void*>(pthread_rwlock_wrlock)}
};
