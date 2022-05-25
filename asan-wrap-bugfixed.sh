#!/system/bin/sh

# It is taken from https://github.com/android/ndk/issues/933

HERE=$(cd "$(dirname "$0")" && pwd)

cmd=$1
shift

export ASAN_OPTIONS=log_to_syslog=false,allow_user_segv_handler=1
ASAN_LIB=$(ls "$HERE"/libclang_rt.asan-*-android.so)
if [ -f "$HERE/libc++_shared.so" ]; then
    # Workaround for https://github.com/android-ndk/ndk/issues/988.
    export LD_PRELOAD="$ASAN_LIB $HERE/libc++_shared.so"
else
    export LD_PRELOAD="$ASAN_LIB"
fi

os_version=$(getprop ro.build.version.sdk)

if [ "$os_version" -eq "27" ]; then
  cmd="$cmd -Xrunjdwp:transport=dt_android_adb,suspend=n,server=y -Xcompiler-option --debuggable $@"
elif [ "$os_version" -eq "28" ]; then
  cmd="$cmd -XjdwpProvider:adbconnection -XjdwpOptions:suspend=n,server=y -Xcompiler-option --debuggable $@"
else
  cmd="$cmd -XjdwpProvider:adbconnection -XjdwpOptions:suspend=n,server=y $@"
fi

exec $cmd

