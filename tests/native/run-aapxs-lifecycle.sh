#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$script_dir/../.." && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/aapxs-regressions.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT
cat > "$build_dir/compat.h" <<'HEADER'
#pragma once
#include <atomic>
#include <ctime>
#ifdef __APPLE__
inline int clock_nanosleep(clockid_t, int, const timespec* delay, timespec* remaining) {
    return nanosleep(delay, remaining);
}
#endif
HEADER
for configuration in debug release; do
    if [ "$configuration" = release ]; then define=-DNDEBUG; else define=-UNDEBUG; fi
    for test in zero-id typed-completion lifecycle metadata connections session-isolation sync-waits; do
        [ -f "$script_dir/aapxs-$test.cpp" ] || continue
        set -- -include "$build_dir/compat.h"
        if [ "$test" = connections ]; then
            set -- "$@" "$repo/androidaudioplugin/src/main/cpp/core/hosting/plugin-connections.cpp"
        fi
        if [ "$test" = metadata ] && [ -f "$script_dir/midi-jni-stub.h" ]; then
            set -- "$@" -include "$script_dir/midi-jni-stub.h" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/midi-aapxs.cpp"
        fi
        "${CXX:-clang++}" -std=c++17 -g -pthread "$define" \
            -fsanitize=undefined -fno-sanitize=vptr "$@" \
            -I "$repo/include" -I "$repo/external/cmidi2" \
            -I "$repo/androidaudioplugin/src/main/cpp/core/hosting" \
            -I "$repo/androidaudioplugin/src/main/cpp/core/aapxs" \
            "$script_dir/aapxs-$test.cpp" \
            "$repo/androidaudioplugin/src/main/cpp/core/hosting/AAPXSMidi2InitiatorSession.cpp" \
            "$repo/androidaudioplugin/src/main/cpp/core/hosting/aap_midi2_helper.cpp" \
            "$repo/androidaudioplugin/src/main/cpp/core/hosting/aapxs-transport.cpp" \
            "$repo/androidaudioplugin/src/main/cpp/core/aapxs/typed-aapxs.cpp" \
            -o "$build_dir/$test-$configuration"
        "$build_dir/$test-$configuration"
    done
done
