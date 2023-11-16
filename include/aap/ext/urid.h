#ifndef AAP_URID_H_INCLUDED
#define AAP_URID_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

#define AAP_URID_EXTENSION_URI "urn://androidaudioplugin.org/extensions/urid/v3"

typedef struct aap_urid_extension_t {
    /*
     * `aapxs_context` is an opaque pointer assigned and used by AAPXS hosting implementation (libandroidaudioplugin).
     * Neither of plugin developer (extension user) or extension developers is supposed to touch it.
     * this struct is instantiated per extension in a plugin instance.
     */
    void *aapxs_context;

    /*
     * Add a mapping from URI to URID.
     * It is an error if this function is called once it started "beginPrepare()" call.
     */
    void (*map) (aap_urid_extension_t* ext, AndroidAudioPlugin* plugin, uint8_t urid, const char* uri);
} aap_urid_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // AAP_URID_H_INCLUDED
