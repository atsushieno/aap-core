#ifndef AAP_PRESETS_H_INCLUDED
#define AAP_PRESETS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

#define AAP_PRESETS_EXTENSION_URI "urn://androidaudioplugin.org/extensions/presets/v1"

#define AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH 256

typedef struct aap_preset_t {
    int32_t index{0};
    char name[AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH];
    void *data;
    int32_t data_size;
} aap_preset_t;

typedef int32_t (*presets_extension_get_preset_count_func_t) (AndroidAudioPluginExtensionTarget target);
typedef int32_t (*presets_extension_get_preset_data_size_func_t) (AndroidAudioPluginExtensionTarget target, int32_t index);
typedef void (*presets_extension_get_preset_func_t) (AndroidAudioPluginExtensionTarget target, int32_t index, bool skipBinary, aap_preset_t *preset);
typedef int32_t (*presets_extension_get_preset_index_func_t) (AndroidAudioPluginExtensionTarget target);
typedef void (*presets_extension_set_preset_index_func_t) (AndroidAudioPluginExtensionTarget target, int32_t index);

typedef struct aap_presets_extension_t {
    RT_UNSAFE presets_extension_get_preset_count_func_t get_preset_count;
    RT_UNSAFE presets_extension_get_preset_data_size_func_t get_preset_data_size;
    RT_UNSAFE presets_extension_get_preset_func_t get_preset;
    RT_UNSAFE presets_extension_get_preset_index_func_t get_preset_index;
    RT_UNSAFE presets_extension_set_preset_index_func_t set_preset_index;
} aap_presets_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_PRESETS_H_INCLUDED */


