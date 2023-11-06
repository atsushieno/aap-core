#ifndef AAP_STATE_H_INCLUDED
#define AAP_STATE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

#define AAP_STATE_EXTENSION_URI "urn://androidaudioplugin.org/extensions/state/v3"

typedef struct {
    void* data;
    size_t data_size;
} aap_state_t;

typedef struct aap_state_extension_t {
    /*
     * `aapxs_context` is an opaque pointer assigned and used by AAPXS hosting implementation (libandroidaudioplugin).
     * Neither of plugin developer (extension user) or extension developers is supposed to touch it.
     * this struct is instantiated per extension in a plugin instance.
     */
    void *aapxs_context;
    RT_UNSAFE size_t (*get_state_size) (aap_state_extension_t* ext, AndroidAudioPlugin* plugin);
    RT_UNSAFE void (*get_state) (aap_state_extension_t* ext, AndroidAudioPlugin* plugin, aap_state_t* destination);
    RT_UNSAFE void (*set_state) (aap_state_extension_t* ext, AndroidAudioPlugin* plugin, aap_state_t* source);
} aap_state_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_STATE_H_INCLUDED */
