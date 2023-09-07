#ifndef AAP_CORE_AAP_MIDI2_HELPER_CPP
#define AAP_CORE_AAP_MIDI2_HELPER_CPP

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#include <cmidi2.h>
#pragma clang diagnostic pop
#include "aap/android-audio-plugin.h"

// C header, not C++. No namespace.

#define AAP_MIDI2_AAPXS_DATA_MAX_SIZE 1024

/**
 * Turns AAPXS extension invocation buffer into MIDI2 UMP.
 *
 * It requires an additional `uint8_t` buffer (`conversionHelperBuffer`) to
 * process the actual conversion without extra memory allocation for RT safety.
 *
 * @param dst                           the destination buffer
 * @param dstSizeInInt                  the size of `dst`
 * @param conversionHelperBuffer        the conversion buffer for temporary storage
 * @param conversionHelperBufferSize    the size of `conversionHelperBuffer`
 * @param group                         "group" field as in MIDI UMP
 * @param uri                           the extension URI (must be null terminated)
 * @param data                          the extension invocation data
 * @param dataSize                      the size of `data`
 * @return `true` if success, `false` for failure (so far insufficient memory only).
 */
[[maybe_unused]] // FIXME: test!
static bool aapMidi2GenerateAAPXSSysEx8(uint32_t* dst,
                                        size_t dstSizeInInt,
                                        uint8_t* conversionHelperBuffer,
                                        size_t conversionHelperBufferSize,
                                        uint8_t group,
                                        const char* uri,
                                        const uint8_t* data,
                                        size_t dataSize);

struct aap_midi2_aapxs_parse_context {
    uint8_t group;
    char uri[AAP_MAX_EXTENSION_URI_SIZE]; // must be null-terminated
    uint8_t *data; // buffer must be allocated by the parsing host
    uint32_t dataSize; // parsed result
    uint8_t* conversionHelperBuffer;
    size_t conversionHelperBufferSize;
};

static inline void aap_midi2_aapxs_parse_context_prepare(
        aap_midi2_aapxs_parse_context* context,
        uint8_t* dataBuffer,
        uint8_t* conversionHelperBuffer,
        size_t conversionHelperBufferSize) {
    context->data = dataBuffer;
    context->dataSize = 0;
    context->conversionHelperBuffer = conversionHelperBuffer;
    context->conversionHelperBufferSize = conversionHelperBufferSize;
}

// Reads AAPXS SysEx8 UMP into `aap_midi2_aapxs_parse_context`.
// Returns true if it is successfully parsed and it turned out to be AAPXS SysEx8.
// In any other case, return false.
bool aap_midi2_parse_aapxs_sysex8(aap_midi2_aapxs_parse_context* context,
                                         uint8_t* umpData,
                                         size_t umpSize);

#endif //AAP_CORE_AAP_MIDI2_HELPER_H
