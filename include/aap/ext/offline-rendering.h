
#ifndef ANDROIDAUDIOPLUGIN_OFFLINE_RENDERING_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_OFFLINE_RENDERING_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AAP_OFFLINE_RENDERING_EXTENSION_URI "urn://androidaudioplugin.org/extensions/offline-rendering/v1"

// Hosts tell plugins whether process() runs against the realtime clock, or faster/slower than that
// (bouncing, freezing). In offline mode a plugin should favor quality and completeness over
// latency: use its best quality algorithms, and wait for its resources instead of dropping work.

// An instance starts in AAP_OFFLINE_RENDERING_MODE_REALTIME and keeps the mode it was last set to,
// including across prepare() and activate()/deactivate().
enum aap_offline_rendering_mode_t {
    AAP_OFFLINE_RENDERING_MODE_REALTIME,
    AAP_OFFLINE_RENDERING_MODE_OFFLINE,
};

enum aap_offline_rendering_result_t {
    AAP_OFFLINE_RENDERING_RESULT_OK,
    AAP_OFFLINE_RENDERING_RESULT_GENERIC_FAILURE,
};

typedef struct aap_offline_rendering_extension_t {
    // AAPXS context, only AAPXS developer touches it.
    // Plugin and host developers should treat it as a reserved field and assign NULL.
    void* aapxs_context;

    // Hosts must call it only while the instance is not active, and only for a plugin that declares
    // this extension. On failure the plugin stays in its current mode, and the framework
    // reports failure without calling the plugin if the instance is active.
    RT_UNSAFE enum aap_offline_rendering_result_t (*set_offline_mode) (
            struct aap_offline_rendering_extension_t* ext,
            AndroidAudioPlugin* plugin,
            enum aap_offline_rendering_mode_t mode);
} aap_offline_rendering_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_OFFLINE_RENDERING_EXTENSION_H_INCLUDED

