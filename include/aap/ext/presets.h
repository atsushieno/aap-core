#ifndef AAP_PRESETS_H_INCLUDED
#define AAP_PRESETS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

#define AAP_PRESETS_EXTENSION_URI "urn://androidaudioplugin.org/extensions/presets/v2"

#define AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH 256

typedef struct aap_preset_t {
    int32_t index{0};
    char name[AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH];
    void *data;
    int32_t data_size;
} aap_preset_t;

typedef struct aap_presets_extension_t {
    void* aapxs_context;
    RT_UNSAFE int32_t (*get_preset_count) (aap_presets_extension_t* ext, AndroidAudioPlugin* plugin);
    RT_UNSAFE int32_t (*get_preset_data_size) (aap_presets_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index);
    RT_UNSAFE void (*get_preset) (aap_presets_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index, bool skipBinary, aap_preset_t *preset);
    RT_UNSAFE int32_t (*get_preset_index) (aap_presets_extension_t* ext, AndroidAudioPlugin* plugin);
    RT_UNSAFE void (*set_preset_index) (aap_presets_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index);
} aap_presets_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_PRESETS_H_INCLUDED */


