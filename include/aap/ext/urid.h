#ifndef AAP_URID_H_INCLUDED
#define AAP_URID_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

/*
 * URID extension provides host a way to tell the target plugin that an integer is mapped to
 * a URI so that the integer could be used as an alternate identifier. Such an integer can be
 * used in realtime context because there will be no need to perform O(n) comparisons.
 *
 * LV2 has the same concept. The API in AAP is similar but less flexible: we do not support unmapping.
 *
 * URID does not assure that the integer is the only value that is mapped to the URI:
 * Multiple URIDs could be mapped to the same URI.
 *
 * Due to the design limitation in AAPXS, the value range for a URID is 1 to 255:
 * 0 is regarded as unmapped, in that case implementations will perform O(n) URI mapping.
 *
 * It should be noted that a host application does not have full control on the mappings.
 * The reference implementation could give arbitrary number of mappings.
 * Also note that they are also consumed by host implementation (service loader), not just the plugin.
 *
 */

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
