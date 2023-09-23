#include "aap/core/aap_midi2_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

static uint32_t aapMidi2ExtensionHelperGetUInt32(uint8_t* dst) {
    if (cmidi2_util_is_platform_little_endian())
        return dst[0] + (dst[1] << 8) + (dst[2] << 16) + (dst[1] << 24);
    else
        return *((uint32_t*) dst);
}

static void aapMidi2ExtensionHelperPutUInt32(uint8_t* dst, uint32_t value) {
    if (cmidi2_util_is_platform_little_endian()) {
        dst[0] = value & 0xFF;
        dst[1] = (value >> 8) & 0xFF;
        dst[2] = (value >> 16) & 0xFF;
        dst[3] = value >> 24;
    }
    else
        *((uint32_t*) dst) = value;
}

uint32_t aap_midi2_aapxs_get_request_id(uint8_t* ump) {
    return aapMidi2ExtensionHelperGetUInt32(ump + 8);
}

static void* aapMidi2ExtensionInvokeHelperSysEx8Forge(uint64_t data1, uint64_t data2, size_t index, void* context) {
    auto forge = (cmidi2_ump_forge*) context;
    if (cmidi2_ump_forge_add_packet_128(forge, data1, data2))
        return nullptr;
    else
        return (void*) true;
}

size_t aap_midi2_generate_aapxs_sysex8(uint32_t* dst,
                                       size_t dstSizeInInt,
                                       uint8_t* conversionHelperBuffer,
                                       size_t conversionHelperBufferSize,
                                       uint8_t group,
                                       uint32_t requestId,
                                       const char* uri,
                                       int32_t opcode,
                                       const uint8_t* data,
                                       size_t dataSize) {
    size_t required = 16 + strlen(uri) + 4 + dataSize * sizeof(uint32_t);
    if (dstSizeInInt * sizeof(int32_t) < required || conversionHelperBufferSize < required)
        return 0;

    uint8_t* sysex = conversionHelperBuffer;
    uint8_t* ptr = sysex;
    uint8_t sysexStart[] {
        0x7Eu, 0x7Fu, // universal sysex
        0, 1, // code
        0 // extension flags
        };
    memcpy(ptr, sysexStart, sizeof(sysexStart));
    ptr += sizeof(sysexStart);
    aapMidi2ExtensionHelperPutUInt32(ptr, requestId);
    ptr += sizeof(uint32_t);

    // uri_size and uri
    size_t strSize = strlen(uri);
    aapMidi2ExtensionHelperPutUInt32(ptr, strSize);
    ptr += sizeof(uint32_t);
    memcpy(ptr, uri, strSize);
    ptr += strSize;

    // opcode
    aapMidi2ExtensionHelperPutUInt32(ptr, (uint32_t) opcode);
    ptr += sizeof(uint32_t);

    // dataSize and data
    aapMidi2ExtensionHelperPutUInt32(ptr, dataSize);
    ptr += sizeof(uint32_t);
    memcpy(ptr, data, dataSize);
    ptr += dataSize;

    // write sysex data to dst.
    cmidi2_ump_forge forge;
    cmidi2_ump_forge_init(&forge, dst, dstSizeInInt * sizeof(int32_t));
    cmidi2_ump_sysex8_process(group, sysex, ptr - sysex, 0,
                              aapMidi2ExtensionInvokeHelperSysEx8Forge, &forge);

    return forge.offset;
}

cmidi2_ump_binary_read_state* sysex8_binary_reader_helper_select_stream(uint8_t targetStreamId, void* context) {
    return (cmidi2_ump_binary_read_state*) context;
}

bool aap_midi2_parse_aapxs_sysex8(aap_midi2_aapxs_parse_context* context,
                                  uint8_t* umpData,
                                  size_t umpSize) {
    cmidi2_ump* ump = (cmidi2_ump*) umpData;
    if (cmidi2_ump_get_message_type(ump) != CMIDI2_MESSAGE_TYPE_SYSEX8_MDS)
        return false;
    if (cmidi2_ump_get_status_code(ump) != CMIDI2_SYSEX_START)
        return false;

    // Retrieve simple binary data array.
    context->group = cmidi2_ump_get_group(ump);
    cmidi2_ump_binary_read_state state;
    cmidi2_ump_binary_read_state_init(&state, nullptr, context->conversionHelperBuffer, context->conversionHelperBufferSize, false);
    if (cmidi2_ump_get_sysex8_data(sysex8_binary_reader_helper_select_stream, &state, nullptr, ump, umpSize / 4) == 0)
        return false;
    if (state.resultCode != CMIDI2_BINARY_READER_RESULT_COMPLETE)
        return false;

    // Now state.data (== context->conversionHelperBuffer) contains the entire SysEx8 buffer.
    // It's time to parse the buffer according to the AAPXS SysEx8:
    // > `[5g sz si 7E]  [7F co-de ext-flag]  [re-se-rv-ed]  [uri-size]  [..uri..]  [value-size]  [..value..]`

    // Check if it is long enough to contain the expected data...
    if (state.dataSize < 24)
        return false;

    uint8_t* data = state.data;
    // Check if this sysex8 is Universal SysEx, and contains code field for AAPXS
    if (data[0] != 0x7E || data[1] != 0x7F || data[2] != 0 || data[3] != 1)
        return false;

    // filling in results...
    size_t requestId = aapMidi2ExtensionHelperGetUInt32(data + 5);
    context->request_id = requestId;

    size_t uriSize = aapMidi2ExtensionHelperGetUInt32(data + 9);
    if (state.dataSize < 17 + uriSize)
        return false;
    memcpy(context->uri, data + 13, uriSize);

    context->opcode = aapMidi2ExtensionHelperGetUInt32(data + 13 + uriSize);

    size_t dataSize = aapMidi2ExtensionHelperGetUInt32(data + 17 + uriSize);
    if (state.dataSize < 21 + uriSize + dataSize)
        return false;
    memcpy(context->data, data + 21 + uriSize, dataSize);
    context->dataSize = dataSize;

    return true;
}

#ifdef __cplusplus
} // extern "C"
#endif
