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
if [ "$(uname -s)" = Darwin ]; then
    "${CXX:-clang++}" -std=c++17 -dynamiclib -undefined dynamic_lookup \
        "$script_dir/rt-lock-guard.cpp" -o "$build_dir/rt-lock-guard.dylib"
    "${CXX:-clang++}" -std=c++17 -dynamiclib -undefined dynamic_lookup -fvisibility=hidden \
        -I "$repo/include" "$script_dir/rt-scope-consumer.cpp" -o "$build_dir/rt-scope-consumer.dylib"
fi
for configuration in debug release; do
    if [ "$configuration" = release ]; then define=-DNDEBUG; else define=-UNDEBUG; fi
    for test in ${AAPXS_TESTS:-zero-id typed-completion request-pool lifecycle metadata connections session-isolation sync-waits binder-completion sized-replies parameter-context instance-contexts state-size event-worker recipient-replies legacy-sender rt-handoffs rt-replies rt-parameters rt-processing}; do
        [ -f "$script_dir/aapxs-$test.cpp" ] || continue
        set -- -include "$build_dir/compat.h"
        if [ "$test" = connections ]; then
            set -- "$@" "$repo/androidaudioplugin/src/main/cpp/core/hosting/plugin-connections.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/hosting/PluginInformation.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/hosting/plugin-information-registry.cpp"
        fi
        if [ "$test" = state-size ]; then
            set -- "$@" "$repo/androidaudioplugin/src/main/cpp/core/aapxs/state-aapxs.cpp"
        fi
        if [ "$test" = parameter-context ]; then
            set -- "$@" "$repo/androidaudioplugin/src/main/cpp/core/aapxs/parameters-aapxs.cpp"
        fi
        if [ "$test" = instance-contexts ]; then
            set -- "$@" -include "$script_dir/midi-jni-stub.h" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/aapxs-runtime.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/standard-extensions.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/parameters-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/presets-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/state-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/midi-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/urid-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/gui-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/buses-aapxs.cpp"
        fi
        if [ "$test" = rt-processing ]; then
            if [ "$(uname -s)" = Darwin ]; then set -- "$@" -Wl,-export_dynamic; fi
            set -- "$@" -DAAP_VERIFY_REALTIME -include "$script_dir/rt-platform-stub.h" \
                "$repo/androidaudioplugin/src/main/cpp/core/hosting/PluginInstance.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/hosting/PluginInstance.Local.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/hosting/PluginInstance.Remote.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/hosting/buffer-layout.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/hosting/PluginHost.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/hosting/PluginInformation.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/hosting/AAPXSMidi2RecipientSession.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/aapxs-runtime.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/standard-extensions.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/parameters-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/presets-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/state-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/midi-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/urid-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/gui-aapxs.cpp" \
                "$repo/androidaudioplugin/src/main/cpp/core/aapxs/buses-aapxs.cpp"
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
            "$repo/androidaudioplugin/src/main/cpp/core/hosting/recipient-aapxs.cpp" \
            "$repo/androidaudioplugin/src/main/cpp/core/aapxs/typed-aapxs.cpp" \
            "$repo/androidaudioplugin/src/main/cpp/core/hosting/realtime.cpp" \
            -o "$build_dir/$test-$configuration"
        if [ "$test" = rt-processing ] && [ "$(uname -s)" = Darwin ]; then
            DYLD_INSERT_LIBRARIES="$build_dir/rt-lock-guard.dylib" \
                AAPXS_RT_SCOPE_DSO_PATH="$build_dir/rt-scope-consumer.dylib" "$build_dir/$test-$configuration"
        else
            "$build_dir/$test-$configuration"
        fi
    done
done
