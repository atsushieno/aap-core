
#ifndef ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_LOGGING_H_DEFINED
#define ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_LOGGING_H_DEFINED 1

#include <stdio.h>
#include <stdarg.h>

#if ANDROID
#include <android/log.h>
#endif

#define logged_android_app_name "AAPHostNative"

enum AAP_LOG_LEVEL {
#if ANDROID
    AAP_LOG_LEVEL_UNKNOWN = ANDROID_LOG_UNKNOWN, // should not be used
    AAP_LOG_LEVEL_DEFAULT = ANDROID_LOG_DEFAULT, // should not be used
    AAP_LOG_LEVEL_VERBOSE = ANDROID_LOG_VERBOSE,
    AAP_LOG_LEVEL_DEBUG = ANDROID_LOG_DEBUG,
    AAP_LOG_LEVEL_INFO = ANDROID_LOG_INFO,
    AAP_LOG_LEVEL_WARN = ANDROID_LOG_WARN,
    AAP_LOG_LEVEL_ERROR = ANDROID_LOG_ERROR,
    AAP_LOG_LEVEL_FATAL = ANDROID_LOG_FATAL
#else
    AAP_LOG_LEVEL_UNKNOWN = 0, // should not be used
    AAP_LOG_LEVEL_DEFAULT,
    AAP_LOG_LEVEL_VERBOSE,
    AAP_LOG_LEVEL_DEBUG,
    AAP_LOG_LEVEL_INFO,
    AAP_LOG_LEVEL_WARN,
    AAP_LOG_LEVEL_ERROR,
    AAP_LOG_LEVEL_FATAL
#endif
};

namespace aap
{

static inline int a_log_vprintf(AAP_LOG_LEVEL logLevel, const char *tag, const char *fmt, va_list ap) {
#if ANDROID
    return __android_log_vprint(logLevel, tag, fmt, ap);
#else
    return vprintf(fmt, ap);
#endif
}

static inline int avprintf(const char *fmt, va_list ap) {
    return a_log_vprintf(AAP_LOG_LEVEL_INFO, logged_android_app_name, fmt, ap);
}

static inline int a_log_f(AAP_LOG_LEVEL logLevel, const char *tag, const char *fmt,...) {
    va_list ap;
    va_start (ap, fmt);
    auto ret = a_log_vprintf(logLevel, tag, fmt, ap);
    va_end(ap);
    return ret;
}

static inline int aprintf(const char *fmt,...) {
    va_list ap;
    va_start (ap, fmt);
    auto ret = a_log_vprintf(AAP_LOG_LEVEL_INFO, logged_android_app_name, fmt, ap);
    va_end(ap);
    return ret;
}

static inline void a_log(AAP_LOG_LEVEL logLevel, const char *tag, const char* s) {
#if ANDROID
    __android_log_print(logLevel, tag, "%s", s);
#else
    puts(s);
#endif
}

static inline void aputs(const char* s) {
    a_log(AAP_LOG_LEVEL_INFO, logged_android_app_name, s);
}

}

#endif // ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_LOGGING_H_DEFINED
