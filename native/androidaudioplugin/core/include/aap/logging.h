#include <stdio.h>
#include <stdarg.h>

#if ANDROID
#include <android/log.h>
#endif

namespace aap
{

int avprintf(const char *fmt, va_list ap);

int aprintf (const char *fmt,...);

void aputs(const char* s);

}
