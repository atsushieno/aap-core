#ifndef AAP_PRESETS_H_INCLUDED
#define AAP_PRESETS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "aap-core.h"
#include <stdint.h>

#define AAP_PRESETS_EXTENSION_URI "urn://androidaudioplugin.org/extensions/presets/v1"

typedef struct {
    int32_t index{0};
    char name[256];
    void *data;
    int32_t data_size;
} aap_preset_t;

typedef struct {
    void *context;
    AndroidAudioPlugin* plugin;
} aap_presets_context_t;

typedef int32_t (*presets_extension_get_preset_count_func_t) (aap_presets_context_t* context);
typedef int32_t (*presets_extension_get_preset_data_size_func_t) (aap_presets_context_t* context, int32_t index);
typedef void (*presets_extension_get_preset_func_t) (aap_presets_context_t* context, int32_t index, bool skipBinary, aap_preset_t *preset);
typedef int32_t (*presets_extension_get_preset_index_func_t) (aap_presets_context_t* context);
typedef void (*presets_extension_set_preset_index_func_t) (aap_presets_context_t* context, int32_t index);

typedef struct aap_presets_extension_t {
    void *context;
    presets_extension_get_preset_count_func_t get_preset_count;
    presets_extension_get_preset_data_size_func_t get_preset_data_size;
    presets_extension_get_preset_func_t get_preset;
    presets_extension_get_preset_index_func_t get_preset_index;
    presets_extension_set_preset_index_func_t set_preset_index;
} aap_presets_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_PRESETS_H_INCLUDED */


