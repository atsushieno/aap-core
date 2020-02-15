
#include "../include/aap/logging.h"

const char* logged_android_app_name = "AAPHostNative";

namespace aap {

int avprintf(const char *fmt, va_list ap)
{
#if ANDROID
    return __android_log_vprint(ANDROID_LOG_INFO, logged_android_app_name, fmt, ap);
#else
    return vprintf(fmt, ap);
#endif
}

int aprintf (const char *fmt,...)
{
    va_list ap;
    va_start (ap, fmt);
    auto ret = avprintf(fmt, ap);
    va_end(ap);
    return ret;
}

void aputs(const char* s)
{
#if ANDROID
    __android_log_print(ANDROID_LOG_INFO, logged_android_app_name, "%s", s);
#else
	puts(s);
#endif
}

}
