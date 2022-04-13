#ifndef AAP_STATE_H_INCLUDED
#define AAP_STATE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#define AAP_STATE_EXTENSION_URI "urn://androidaudioplugin.org/extensions/state/v1"

typedef struct {
    void* data;
    int32_t data_size;
} aap_state_t;

typedef struct {
    void get_state(aap_state_t *result);
    void set_state(aap_state_t *state);
} aap_state_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_STATE_H_INCLUDED */



