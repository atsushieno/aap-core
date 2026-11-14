#ifndef AAP_TAIL_H_INCLUDED
#define AAP_TAIL_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

#define AAP_TAIL_EXTENSION_URI "urn://androidaudioplugin.org/extensions/tail/v1"

// The tail is how long a plugin keeps producing output after its input becomes silent (reverb
// decay, delay feedback, release stages). 0 means no tail, and INFINITY means it never ends.
// Hosts may cache the length until the plugin notifies a change through the host extension.

typedef struct aap_tail_extension_t {
    // `aapxs_context` is an opaque pointer assigned and used by AAPXS hosting implementation
    // (libandroidaudioplugin). Plugin and host developers should treat it as reserved.
    void *aapxs_context;
    RT_SAFE double (*tail_length_in_seconds) (struct aap_tail_extension_t* ext, AndroidAudioPlugin* plugin);
} aap_tail_extension_t;

typedef struct aap_tail_host_extension_t {
    void* aapxs_context;
    // Plugins call it whenever the tail length changes, e.g. after a parameter change.
    RT_SAFE void (*notify_tail_length_changed) (struct aap_tail_host_extension_t* ext, AndroidAudioPluginHost* host);
} aap_tail_host_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // AAP_TAIL_H_INCLUDED
