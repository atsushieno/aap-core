#ifndef AAP_PRESETS_H_INCLUDED
#define AAP_PRESETS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

#define AAP_PRESETS_EXTENSION_URI "urn://androidaudioplugin.org/extensions/presets/v3"

/*

  AAP Presets extension is to provide the plugin-defined or user-defined set of State binaries
  (see State extension for details).
  This extension offers *read-only* accesses to them; "generating" presets should be done
  by other means e.g. using State extension.

  ### Preset Index

  This extension, namely since v3, provides instant (non-persistent) access to the presets
  by index, which is NOT persistent across instances (prior to v3 there was no indication).
  That is, next time you instantiate the same plugin, the preset contents by the same index
  may be different.
  Therefore, both plugin and host developers should not treat the preset indices in persistent
  context e.g. a member within state content.
  If it remembers the preset, it should store the actual state or preset "ID" kind of stuff
  (which this extension does not particularly define).

  If you still think you can treat the preset index as persistent, you should make it persistent
  i.e. you cannot make any changes to the actual state content, in the future versions *forever*.

  ### Notifying preset changes

  If there are changes in the presets (such as, user loaded the presets catalog file, change
  presets location, update preset contents on the UI, etc.) that would affect the properties
  that were exposed by those extension functions (e.g. some preset has the name updated,
  the total number of presets has changed, etc.), then the plugin should notify the host
  about the update, using `aap_presets_host_extension_t.notify_presets_update()`.

  Unless it is clearly notified, host can treat information it retrieved via the extension
  functions (preset count, and preset properties) as unchanged e.g. it does not have to
  always query the latest status. It will significantly reduce the need for querying across
  processes, especially Jetpack Compose UI can simply keep the initial state value.

  It should also be noted that not all hosts support the host extension function.

 */

#define AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH 256

typedef struct aap_preset_t {
    // a persistent ID.
    // It should be usable forever across instancing if it is not a user preset.
    // It does not have to match the preset "index"; the index is just an index within the collection.
    // Unlike index, there might be skipped numbers.
    int32_t id{0};

    // preset name, might not be unique even within a plugin
    char name[AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH];
} aap_preset_t;

typedef struct aap_presets_extension_t {
    // AAPXS context, only AAPXS developer touches it.
    // Plugin and host developers should treat it as a reserved field and assign NULL.
    void* aapxs_context;

    RT_SAFE int32_t (*get_preset_count) (aap_presets_extension_t* ext, AndroidAudioPlugin* plugin);
    RT_UNSAFE void (*get_preset) (aap_presets_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index, aap_preset_t *preset, aapxs_completion_callback aapxsCallback, void* callbackData);
    RT_SAFE int32_t (*get_preset_index) (aap_presets_extension_t* ext, AndroidAudioPlugin* plugin);
    RT_UNSAFE void (*set_preset_index) (aap_presets_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index);
} aap_presets_extension_t;

typedef struct aap_presets_host_extension_t {
    void (*notify_preset_loaded) (aap_presets_host_extension_t* ext, AndroidAudioPluginHost* host);
    void (*notify_presets_update) (aap_presets_host_extension_t* ext, AndroidAudioPluginHost* host);
} aap_presets_host_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_PRESETS_H_INCLUDED */


