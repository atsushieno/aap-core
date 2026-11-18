#ifndef AAP_LATENCY_H_INCLUDED
#define AAP_LATENCY_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

#define AAP_LATENCY_EXTENSION_URI "urn://androidaudioplugin.org/extensions/latency/v1"

// The latency is how many samples the plugin delays its output relative to its input (lookahead,
// linear-phase filters, FFT blocks). Hosts may cache it until the plugin notifies a change.

typedef struct aap_latency_extension_t {
    // `aapxs_context` is an opaque pointer assigned and used by AAPXS hosting implementation
    // (libandroidaudioplugin). Plugin and host developers should treat it as reserved.
    void *aapxs_context;
    RT_UNSAFE int32_t (*latency_in_samples) (struct aap_latency_extension_t* ext, AndroidAudioPlugin* plugin);
} aap_latency_extension_t;

typedef struct aap_latency_host_extension_t {
    void* aapxs_context;
    // Plugins call it whenever the latency changes, e.g. after prepare() or a parameter change.
    RT_UNSAFE void (*notify_latency_changed) (struct aap_latency_host_extension_t* ext, AndroidAudioPluginHost* host);
} aap_latency_host_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // AAP_LATENCY_H_INCLUDED
