
#ifndef ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_LOGGING_H_DEFINED
#define ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_LOGGING_H_DEFINED 1

#include <stdio.h>
#include <stdarg.h>

#if ANDROID
#include <android/log.h>
#endif

#define logged_android_app_name "AAPHostNative"

namespace aap
{

static inline int avprintf(const char *fmt, va_list ap)
{
#if ANDROID
    return __android_log_vprint(ANDROID_LOG_INFO, logged_android_app_name, fmt, ap);
#else
    return vprintf(fmt, ap);
#endif
}

static inline int aprintf (const char *fmt,...)
{
    va_list ap;
    va_start (ap, fmt);
    auto ret = avprintf(fmt, ap);
    va_end(ap);
    return ret;
}

static inline void aputs(const char* s)
{
#if ANDROID
    __android_log_print(ANDROID_LOG_INFO, logged_android_app_name, "%s", s);
#else
    puts(s);
#endif
}

}

#endif // ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_LOGGING_H_DEFINED
