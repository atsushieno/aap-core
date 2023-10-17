#ifndef ANDROIDAUDIOPLUGIN_PLUGIN_INFO_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_PLUGIN_INFO_EXTENSION_H_INCLUDED

// plugin-info extension can be used tentatively by a plugin to retrieve its own metadata, ports, and
// parameters from the host that had parsed `aap_metadata.xml` and configured ports etc.
// Those information items are not obviously known to plugin wrappers, including our own local host
// unless it implements `get_plugin_info()` (newly introduced in post-0.7.8 API)
//
// IPC is not involved for `get()`, as it is implemented at LocalPluginInstance. A local plugin host should be
// able to resolve all the plugin information items.

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

#define AAP_PLUGIN_INFO_EXTENSION_URI "urn://androidaudioplugin.org/extensions/plugin-info/v3"

typedef struct aap_host_plugin_info_extension_t {
    aap_plugin_info_t (*get) (aap_host_plugin_info_extension_t* ext, AndroidAudioPluginHost* host, const char *pluginId);
} aap_host_plugin_info_extension_t;

#ifdef __cplusplus
}
#endif

#endif // ANDROIDAUDIOPLUGIN_PLUGIN_INFO_EXTENSION_H_INCLUDED
