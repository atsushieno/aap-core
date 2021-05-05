#include <stdbool.h>
#include <stdint.h>
#include <memory.h>

#define MIDI_2_0_RESERVED 0
#define JR_TIMESTAMP_TICKS_PER_SECOND 31250

#ifdef __cplusplus
extern "C" {
#endif

enum cmidi2_status_code {
    CMIDI2_STATUS_NOTE_OFF = 0x80,
    CMIDI2_STATUS_NOTE_ON = 0x90,
    CMIDI2_STATUS_PAF = 0xA0,
    CMIDI2_STATUS_CC = 0xB0,
    CMIDI2_STATUS_PROGRAM = 0xC0,
    CMIDI2_STATUS_CAF = 0xD0,
    CMIDI2_STATUS_PITCH_BEND = 0xE0,
    CMIDI2_STATUS_PER_NOTE_RCC = 0x00,
    CMIDI2_STATUS_PER_NOTE_ACC = 0x10,
    CMIDI2_STATUS_RPN = 0x20,
    CMIDI2_STATUS_NRPN = 0x30,
    CMIDI2_STATUS_RELATIVE_RPN = 0x40,
    CMIDI2_STATUS_RELATIVE_NRPN = 0x50,
    CMIDI2_STATUS_PER_NOTE_PITCH_BEND = 0x60,
    CMIDI2_STATUS_PER_NOTE_MANAGEMENT = 0xF0,
};

enum cmidi2_message_type {
    // MIDI 2.0 UMP Section 3.
    CMIDI2_MESSAGE_TYPE_UTILITY = 0,
    CMIDI2_MESSAGE_TYPE_SYSTEM = 1,
    CMIDI2_MESSAGE_TYPE_MIDI_1_CHANNEL = 2,
    CMIDI2_MESSAGE_TYPE_SYSEX7 = 3,
    CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL = 4,
    CMIDI2_MESSAGE_TYPE_SYSEX8_MDS = 5,
};

enum cmidi2_ci_protocol_bytes {
    CMIDI2_PROTOCOL_BYTES_TYPE = 1,
    CMIDI2_PROTOCOL_BYTES_VERSION = 2,
    CMIDI2_PROTOCOL_BYTES_EXTENSIONS = 3,
};

enum cmidi2_ci_protocol_values {
    CMIDI2_PROTOCOL_TYPE_MIDI1 = 1,
    CMIDI2_PROTOCOL_TYPE_MIDI2 = 2,
    CMIDI2_PROTOCOL_VERSION_MIDI1 = 0,
    CMIDI2_PROTOCOL_VERSION_MIDI2_V1 = 0,
};

enum cmidi2_ci_protocol_extensions {
    CMIDI2_PROTOCOL_EXTENSIONS_JITTER = 1,
    CMIDI2_PROTOCOL_EXTENSIONS_LARGER = 2, // only for MIDI 1.0 compat UMP
};

typedef struct cmidi2_ci_protocol_tag {
    uint8_t protocol_type; // cmidi2_ci_protocol_bytes
    uint8_t version; // cmidi2_ci_protocol_values
    uint8_t extensions; // cmidi2_ci_protocol_extensions (flags)
    uint8_t reserved1;
    uint8_t reserved2;
} cmidi2_ci_protocol;

enum cmidi2_per_note_management_flags {
    CMIDI2_PER_NOTE_MANAGEMENT_RESET = 1,
    CMIDI2_PER_NOTE_MANAGEMENT_DETACH = 2,
};

enum cmidi2_note_attribute_type {
    CMIDI2_ATTRIBUTE_TYPE_NONE = 0,
    CMIDI2_ATTRIBUTE_TYPE_MANUFACTURER = 1,
    CMIDI2_ATTRIBUTE_TYPE_PROFILE = 2,
    CMIDI2_ATTRIBUTE_TYPE_PITCH7_9 = 3,
};

enum cmidi2_sysex_status {
    CMIDI2_SYSEX_IN_ONE_UMP = 0,
    CMIDI2_SYSEX_START = 0x10,
    CMIDI2_SYSEX_CONTINUE = 0x20,
    CMIDI2_SYSEX_END = 0x30,
};

enum cmidi2_mixed_data_set_status {
    CMIDI2_MIXED_DATA_STATUS_HEADER = 0x80,
    CMIDI2_MIXED_DATA_STATUS_PAYLOAD = 0x90,
};

enum cmidi2_system_message_status {
    CMIDI2_SYSTEM_STATUS_MIDI_TIME_CODE = 0xF1,
    CMIDI2_SYSTEM_STATUS_SONG_POSITION = 0xF2,
    CMIDI2_SYSTEM_STATUS_SONG_SELECT = 0xF3,
    CMIDI2_SYSTEM_STATUS_TUNE_REQUEST = 0xF6,
    CMIDI2_SYSTEM_STATUS_TIMING_CLOCK = 0xF8,
    CMIDI2_SYSTEM_STATUS_START = 0xFA,
    CMIDI2_SYSTEM_STATUS_CONTINUE = 0xFB,
    CMIDI2_SYSTEM_STATUS_STOP = 0xFC,
    CMIDI2_SYSTEM_STATUS_ACTIVE_SENSING = 0xFE,
    CMIDI2_SYSTEM_STATUS_RESET = 0xFF,
};

enum cmidi2_jr_timestamp_status {
    CMIDI2_JR_CLOCK = 0x10,
    CMIDI2_JR_TIMESTAMP = 0x20,
};

static inline uint8_t cmidi2_ump_get_num_bytes(uint32_t data) {
    switch (((data & 0xF0000000) >> 28) & 0xF) {
    case CMIDI2_MESSAGE_TYPE_UTILITY:
    case CMIDI2_MESSAGE_TYPE_SYSTEM:
    case CMIDI2_MESSAGE_TYPE_MIDI_1_CHANNEL:
        return 4;
    case CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL:
    case CMIDI2_MESSAGE_TYPE_SYSEX7:
        return 8;
    case CMIDI2_MESSAGE_TYPE_SYSEX8_MDS:
        return 16;
    }
    return 0xFF; /* wrong */
}

// 4.8 Utility Messages
static inline uint32_t cmidi2_ump_noop(uint8_t group) { return (group & 0xF) << 24; }

static inline uint32_t cmidi2_ump_jr_clock_direct(uint8_t group, uint32_t senderClockTime) {
        return cmidi2_ump_noop(group) + (CMIDI2_JR_CLOCK << 16) + senderClockTime;
}

static inline uint32_t cmidi2_ump_jr_clock(uint8_t group, double senderClockTime) {
    uint16_t value = (uint16_t) (senderClockTime * JR_TIMESTAMP_TICKS_PER_SECOND);
    return cmidi2_ump_noop(group) + (CMIDI2_JR_CLOCK << 16) + value;
}

static inline uint32_t cmidi2_ump_jr_timestamp_direct(uint8_t group, uint32_t senderClockTimestamp) {
        return cmidi2_ump_noop(group) + (CMIDI2_JR_TIMESTAMP << 16) + senderClockTimestamp;
}

static inline uint32_t cmidi2_ump_jr_timestamp(uint8_t group, double senderClockTimestamp) {
    uint16_t value = (uint16_t) (senderClockTimestamp * JR_TIMESTAMP_TICKS_PER_SECOND);
    return cmidi2_ump_noop(group) + (CMIDI2_JR_TIMESTAMP << 16) + value;
}

// 4.3 System Common and System Real Time Messages
static inline int32_t cmidi2_ump_system_message(uint8_t group, uint8_t status, uint8_t midi1Byte2, uint8_t midi1Byte3) {
    return (CMIDI2_MESSAGE_TYPE_SYSTEM << 28) + ((group & 0xF) << 24) + (status << 16) + ((midi1Byte2 & 0x7F) << 8) + (midi1Byte3 & 0x7F);
}

// 4.1 MIDI 1.0 Channel Voice Messages
static inline int32_t cmidi2_ump_midi1_message(uint8_t group, uint8_t code, uint8_t channel, uint8_t byte3, uint8_t byte4) {
    return (CMIDI2_MESSAGE_TYPE_MIDI_1_CHANNEL << 28) + ((group & 0xF) << 24) + (((code & 0xF0) + (channel & 0xF)) << 16) + ((byte3 & 0x7F) << 8) + (byte4 & 0x7F);
}

static inline int32_t cmidi2_ump_midi1_note_off(uint8_t group, uint8_t channel, uint8_t note, uint8_t velocity) {
    return cmidi2_ump_midi1_message(group, CMIDI2_STATUS_NOTE_OFF, channel, note & 0x7F, velocity & 0x7F);
}

static inline int32_t cmidi2_ump_midi1_note_on(uint8_t group, uint8_t channel, uint8_t note, uint8_t velocity) {
    return cmidi2_ump_midi1_message(group, CMIDI2_STATUS_NOTE_ON, channel, note & 0x7F, velocity & 0x7F);
}

static inline int32_t cmidi2_ump_midi1_paf(uint8_t group, uint8_t channel, uint8_t note, uint8_t data) {
    return cmidi2_ump_midi1_message(group, CMIDI2_STATUS_PAF, channel, note & 0x7F, data & 0x7F);
}

static inline int32_t cmidi2_ump_midi1_cc(uint8_t group, uint8_t channel, uint8_t index, uint8_t data) {
    return cmidi2_ump_midi1_message(group, CMIDI2_STATUS_CC, channel, index & 0x7F, data & 0x7F);
}

static inline int32_t cmidi2_ump_midi1_program(uint8_t group, uint8_t channel, uint8_t program) {
    return cmidi2_ump_midi1_message(group, CMIDI2_STATUS_PROGRAM, channel, program & 0x7F, MIDI_2_0_RESERVED);
}

static inline int32_t cmidi2_ump_midi1_caf(uint8_t group, uint8_t channel, uint8_t data) {
    return cmidi2_ump_midi1_message(group, CMIDI2_STATUS_CAF, channel, data & 0x7F, MIDI_2_0_RESERVED);
}

static inline int32_t cmidi2_ump_midi1_pitch_bend_direct(uint8_t group, uint8_t channel, uint16_t data) {
    return cmidi2_ump_midi1_message(group, CMIDI2_STATUS_PITCH_BEND, channel, data & 0x7F, (data >> 7) & 0x7F);
}

static inline int32_t cmidi2_ump_midi1_pitch_bend_split(uint8_t group, uint8_t channel, uint8_t dataLSB, uint8_t dataMSB) {
    return cmidi2_ump_midi1_message(group, CMIDI2_STATUS_PITCH_BEND, channel, dataLSB & 0x7F, dataMSB & 0x7F);
}

static inline int32_t cmidi2_ump_midi1_pitch_bend(uint8_t group, uint8_t channel, int16_t data) {
    data += 8192;
    return cmidi2_ump_midi1_message(group, CMIDI2_STATUS_PITCH_BEND, channel, data & 0x7F, (data >> 7) & 0x7F);
}

// 4.2 MIDI 2.0 Channel Voice Messages
static inline int64_t cmidi2_ump_midi2_channel_message_8_8_16_16(
    uint8_t group, uint8_t code, uint8_t channel, uint8_t byte3, uint8_t byte4,
    uint16_t short1, uint16_t short2) {
    return (((uint64_t) (CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL << 28) + ((group & 0xF) << 24) + (((code & 0xF0) + (channel & 0xF)) << 16) + (byte3 << 8) + byte4) << 32) + ((uint64_t) short1 << 16) + short2;
}

static inline int64_t cmidi2_ump_midi2_channel_message_8_8_32(
    uint8_t group, uint8_t code, uint8_t channel, uint8_t byte3, uint8_t byte4,
    uint32_t rest) {
    return (((uint64_t) (CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL << 28) + ((group & 0xF) << 24) + (((code & 0xF0) + (channel & 0xF)) << 16) + (byte3 << 8) + byte4) << 32) + rest;
}

static inline uint16_t cmidi2_ump_pitch_7_9(double semitone) {
    double actual = semitone < 0.0 ? 0.0 : semitone >= 128.0 ? 128.0 : semitone;
    uint16_t dec = (uint16_t) actual;
    double microtone = actual - dec;
    return (dec << 9) + (int) (microtone * 512.0);
}

static inline uint16_t cmidi2_ump_pitch_7_9_split(uint8_t semitone, double microtone) {
    uint16_t ret = (uint16_t) (semitone & 0x7F) << 9;
    double actual = microtone < 0.0 ? 0.0 : microtone > 1.0 ? 1.0 : microtone;
    ret += (int) (actual * 512.0);
    return ret;
}

static inline int64_t cmidi2_ump_midi2_note_off(uint8_t group, uint8_t channel, uint8_t note, uint8_t attributeType, uint16_t velocity, uint16_t attributeData) {
    return cmidi2_ump_midi2_channel_message_8_8_16_16(group, CMIDI2_STATUS_NOTE_OFF, channel, note & 0x7F, attributeType, velocity, attributeData);
}

static inline int64_t cmidi2_ump_midi2_note_on(uint8_t group, uint8_t channel, uint8_t note, uint8_t attributeType, uint16_t velocity, uint16_t attributeData) {
    return cmidi2_ump_midi2_channel_message_8_8_16_16(group, CMIDI2_STATUS_NOTE_ON, channel, note & 0x7F, attributeType, velocity, attributeData);
}

static inline int64_t cmidi2_ump_midi2_paf(uint8_t group, uint8_t channel, uint8_t note, uint32_t data) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_PAF, channel, note & 0x7F, MIDI_2_0_RESERVED, data);
}

static inline int64_t cmidi2_ump_midi2_per_note_rcc(uint8_t group, uint8_t channel, uint8_t note, uint8_t index, uint32_t data) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_PER_NOTE_RCC, channel, note & 0x7F, index, data);
}

static inline int64_t cmidi2_ump_midi2_per_note_acc(uint8_t group, uint8_t channel, uint8_t note, uint8_t index, uint32_t data) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_PER_NOTE_ACC, channel, note & 0x7F, index, data);
}

static inline int64_t cmidi2_ump_midi2_per_note_management(uint8_t group, uint8_t channel, uint8_t note, uint8_t optionFlags) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_PER_NOTE_MANAGEMENT, channel, note & 0x7F, optionFlags & 3, 0);
}

static inline int64_t cmidi2_ump_midi2_cc(uint8_t group, uint8_t channel, uint8_t index, uint32_t data) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_CC, channel, index & 0x7F, MIDI_2_0_RESERVED, data);
}

static inline int64_t cmidi2_ump_midi2_rpn(uint8_t group, uint8_t channel, uint8_t bankAkaMSB, uint8_t indexAkaLSB, uint32_t dataAkaDTE) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_RPN, channel, bankAkaMSB & 0x7F, indexAkaLSB & 0x7F, dataAkaDTE);
}

static inline int64_t cmidi2_ump_midi2_nrpn(uint8_t group, uint8_t channel, uint8_t bankAkaMSB, uint8_t indexAkaLSB, uint32_t dataAkaDTE) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_NRPN, channel, bankAkaMSB & 0x7F, indexAkaLSB & 0x7F, dataAkaDTE);
}

static inline int64_t cmidi2_ump_midi2_relative_rpn(uint8_t group, uint8_t channel, uint8_t bankAkaMSB, uint8_t indexAkaLSB, uint32_t dataAkaDTE) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_RELATIVE_RPN, channel, bankAkaMSB & 0x7F, indexAkaLSB & 0x7F, dataAkaDTE);
}

static inline int64_t cmidi2_ump_midi2_relative_nrpn(uint8_t group, uint8_t channel, uint8_t bankAkaMSB, uint8_t indexAkaLSB, uint32_t dataAkaDTE) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_RELATIVE_NRPN, channel, bankAkaMSB & 0x7F, indexAkaLSB & 0x7F, dataAkaDTE);
}

static inline int64_t cmidi2_ump_midi2_program(uint8_t group, uint8_t channel, uint8_t optionFlags, uint8_t program, uint8_t bankMSB, uint8_t bankLSB) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_PROGRAM, channel, MIDI_2_0_RESERVED, optionFlags & 1,
        ((program & 0x7F) << 24) + (bankMSB << 8) + bankLSB);
}

static inline int64_t cmidi2_ump_midi2_caf(uint8_t group, uint8_t channel, uint32_t data) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_CAF, channel, MIDI_2_0_RESERVED, MIDI_2_0_RESERVED, data);
}

static inline int64_t cmidi2_ump_midi2_pitch_bend_direct(uint8_t group, uint8_t channel, uint32_t unsignedData) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_PITCH_BEND, channel, MIDI_2_0_RESERVED, MIDI_2_0_RESERVED, unsignedData);
}

static inline int64_t cmidi2_ump_midi2_pitch_bend(uint8_t group, uint8_t channel, int32_t data) {
    return cmidi2_ump_midi2_pitch_bend_direct(group, channel, 0x80000000 + data);
}

static inline int64_t cmidi2_ump_midi2_per_note_pitch_bend_direct(uint8_t group, uint8_t channel, uint8_t note, uint32_t data) {
    return cmidi2_ump_midi2_channel_message_8_8_32(group, CMIDI2_STATUS_PER_NOTE_PITCH_BEND, channel, note & 0x7F, MIDI_2_0_RESERVED, data);
}

static inline int64_t cmidi2_ump_midi2_per_note_pitch_bend(uint8_t group, uint8_t channel, uint8_t note, uint32_t data) {
    return cmidi2_ump_midi2_per_note_pitch_bend_direct(group, channel, note, 0x80000000 + data);
}

// Common utility functions for sysex support

static inline uint8_t cmidi2_ump_get_byte_from_uint32(uint32_t src, uint8_t index) {
    return (uint8_t) (src >> ((7 - index) * 8) & 0xFF);
}

static inline uint8_t cmidi2_ump_get_byte_from_uint64(uint32_t src, uint8_t index) {
    return (uint8_t) (src >> ((7 - index) * 8) & 0xFF);
}

static inline uint8_t cmidi2_ump_sysex_get_num_packets(uint8_t numBytes, uint8_t radix) {
    return numBytes <= radix ? 1 : numBytes / radix + (numBytes % radix ? 1 : 0);
}

static inline uint32_t cmidi2_ump_read_uint32_bytes(void *sequence) {
    uint8_t *bytes = (uint8_t*) sequence;
    uint32_t ret = 0;
    for (int i = 0; i < 4; i++)
        ret += ((uint32_t) bytes[i]) << ((7 - i) * 8);
    return ret;
}

static inline uint64_t cmidi2_ump_read_uint64_bytes(void *sequence) {
    uint8_t *bytes = (uint8_t*) sequence;
    uint64_t ret = 0;
    for (int i = 0; i < 8; i++)
        ret += ((uint64_t) bytes[i]) << ((7 - i) * 8);
    return ret;
}

static inline void cmidi2_ump_sysex_get_packet_of(uint64_t* result1, uint64_t* result2, uint8_t group, uint8_t numBytes, void* srcData, int32_t index,
        enum cmidi2_message_type messageType, int radix, bool hasStreamId, uint8_t streamId) {
    uint8_t dst8[16];
    memset(dst8, 0, 16);
    uint8_t *src8 = (uint8_t*) srcData;

    dst8[0] = (messageType << 4) + (group & 0xF);

    enum cmidi2_sysex_status status;
    uint8_t size;
    if (numBytes <= radix) {
        status =  CMIDI2_SYSEX_IN_ONE_UMP;
        size = numBytes; // single packet message
    } else if (index == 0) {
        status = CMIDI2_SYSEX_START;
        size = radix;
    } else {
        uint8_t isEnd = index == cmidi2_ump_sysex_get_num_packets(numBytes, radix) - 1;
        if (isEnd) {
            size = numBytes % radix ? numBytes % radix : radix;
            status = CMIDI2_SYSEX_END;
        } else {
            size = radix;
            status = CMIDI2_SYSEX_CONTINUE;
        }
    }
    dst8[1] = status + size + (hasStreamId ? 1 : 0);

    if (hasStreamId)
        dst8[2] = streamId;

    uint8_t dstOffset = hasStreamId ? 3 : 2;
    for (uint8_t i = 0, j = index * radix; i < size; i++, j++)
        dst8[i + dstOffset] = src8[j];

    *result1 = cmidi2_ump_read_uint64_bytes(dst8);
    if (result2)
        *result2 = cmidi2_ump_read_uint64_bytes(dst8 + 8);
}

// 4.4 System Exclusive 7-Bit Messages
static inline uint64_t cmidi2_ump_sysex7_direct(uint8_t group, uint8_t status, uint8_t numBytes, uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4, uint8_t data5, uint8_t data6) {
    return (((uint64_t) ((CMIDI2_MESSAGE_TYPE_SYSEX7 << 28) + ((group & 0xF) << 24) + ((status + numBytes) << 16))) << 32) +
        ((uint64_t) data1 << 40) + ((uint64_t) data2 << 32) + (data3 << 24) + (data4 << 16) + (data5 << 8) + data6;
}

static inline uint32_t cmidi2_ump_sysex7_get_sysex_length(void* srcData) {
    int i = 0;
    uint8_t* csrc = (uint8_t*) srcData;
    while (csrc[i] != 0xF7)        
        i++;
    /* This function automatically detects if 0xF0 is prepended and reduce length if it is. */
    return i - (csrc[0] == 0xF0 ? 1 : 0);
}

static inline uint8_t cmidi2_ump_sysex7_get_num_packets(uint8_t numSysex7Bytes) {
    return cmidi2_ump_sysex_get_num_packets(numSysex7Bytes, 6);
}

static inline uint64_t cmidi2_ump_sysex7_get_packet_of(uint8_t group, uint8_t numBytes, void* srcData, int32_t index) {
    uint64_t result;
    int srcOffset = numBytes > 0 && ((uint8_t*) srcData)[0] == 0xF0 ? 1 : 0;
    cmidi2_ump_sysex_get_packet_of(&result, NULL, group, numBytes, (uint8_t*) srcData + srcOffset, index, CMIDI2_MESSAGE_TYPE_SYSEX7, 6, false, 0);
    return result;
}

/* process() - more complicated function */
typedef void(*cmidi2_ump_handler_u64)(uint64_t data, void* context);

static inline void cmidi2_ump_sysex7_process(uint8_t group, void* sysex, cmidi2_ump_handler_u64 sendUMP, void* context)
{
    int32_t length = cmidi2_ump_sysex7_get_sysex_length(sysex);
    int32_t numPackets = cmidi2_ump_sysex7_get_num_packets(length);
    for (int p = 0; p < numPackets; p++) {
        int64_t ump = cmidi2_ump_sysex7_get_packet_of(group, length, sysex, p);
        sendUMP(ump, context);
    }
}

// 4.5 System Exclusive 8-Bit Messages

static inline int8_t cmidi2_ump_sysex8_get_num_packets(uint8_t numBytes) {
    return cmidi2_ump_sysex_get_num_packets(numBytes, 13);
}

static inline void cmidi2_ump_sysex8_get_packet_of(uint8_t group, uint8_t streamId, uint8_t numBytes, void* srcData, int32_t index, uint64_t* result1, uint64_t* result2) {
    cmidi2_ump_sysex_get_packet_of(result1, result2, group, numBytes, srcData, index, CMIDI2_MESSAGE_TYPE_SYSEX8_MDS, 13, true, streamId);
}

/* process() - more complicated function */
typedef void(*cmidi2_ump_handler_u128)(uint64_t data1, uint64_t data2, size_t index, void* context);

static inline void cmidi2_ump_sysex8_process(uint8_t group, void* sysex, uint32_t length, uint8_t streamId, cmidi2_ump_handler_u128 sendUMP, void* context)
{
    int32_t numPackets = cmidi2_ump_sysex8_get_num_packets(length);
    for (size_t p = 0; p < numPackets; p++) {
        uint64_t result1, result2;
        cmidi2_ump_sysex8_get_packet_of(group, streamId, length, sysex, p, &result1, &result2);
        sendUMP(result1, result2, p, context);
    }
}

// 4.6 Mixed Data Set Message ... should we come up with complicated chunk splitter function?


static inline uint16_t cmidi2_ump_mds_get_num_chunks(uint32_t numTotalBytesInMDS) {
    uint32_t radix = 14 * 0x10000;
    return numTotalBytesInMDS / radix + (numTotalBytesInMDS % radix ? 1 : 0);
}

// Returns -1 if input is out of range
static inline int32_t cmidi2_ump_mds_get_num_payloads(uint32_t numTotalBytesinChunk) {
    if (numTotalBytesinChunk > 14 * 65535)
        return -1;
    return numTotalBytesinChunk / 14 + (numTotalBytesinChunk % 14 ? 1 : 0);
}

static inline void cmidi2_ump_mds_get_header(uint8_t group, uint8_t mdsId,
    uint16_t numBytesInChunk, uint16_t numChunks, uint16_t chunkIndex,
    uint16_t manufacturerId, uint16_t deviceId, uint16_t subId, uint16_t subId2,
    uint64_t* result1, uint64_t* result2) {
    uint8_t dst8[16];
    memset(dst8, 0, 16);

    dst8[0] = (CMIDI2_MESSAGE_TYPE_SYSEX8_MDS << 4) + (group & 0xF);
    dst8[1] = CMIDI2_MIXED_DATA_STATUS_HEADER + mdsId;
    *((uint16_t*) (void*) (dst8 + 2)) = numBytesInChunk;
    *((uint16_t*) (void*) (dst8 + 4)) = numChunks;
    *((uint16_t*) (void*) (dst8 + 6)) = chunkIndex;
    *((uint16_t*) (void*) (dst8 + 8)) = manufacturerId;
    *((uint16_t*) (void*) (dst8 + 10)) = deviceId;
    *((uint16_t*) (void*) (dst8 + 12)) = subId;
    *((uint16_t*) (void*) (dst8 + 14)) = subId2;

    *result1 = cmidi2_ump_read_uint64_bytes(dst8);
    if (result2)
        *result2 = cmidi2_ump_read_uint64_bytes(dst8 + 8);
}

// srcData points to exact start of the source data.
static inline void cmidi2_ump_mds_get_payload_of(uint8_t group, uint8_t mdsId, uint16_t numBytes, void* srcData, uint64_t* result1, uint64_t* result2) {
    uint8_t dst8[16];
    memset(dst8, 0, 16);
    uint8_t *src8 = (uint8_t*) srcData;

    dst8[0] = (CMIDI2_MESSAGE_TYPE_SYSEX8_MDS << 4) + (group & 0xF);
    dst8[1] = CMIDI2_MIXED_DATA_STATUS_PAYLOAD + mdsId;

    uint8_t radix = 14;
    uint8_t size = numBytes < radix ? numBytes % radix : radix;

    for (uint8_t i = 0, j = 0; i < size; i++, j++)
        dst8[i + 2] = src8[j];

    *result1 = cmidi2_ump_read_uint64_bytes(dst8);
    if (result2)
        *result2 = cmidi2_ump_read_uint64_bytes(dst8 + 8);
}

/* process() - more complicated function */
typedef void(*cmidi2_mds_handler)(uint64_t data1, uint64_t data2, size_t chunkId, size_t payloadId, void* context);

static inline void cmidi2_ump_mds_process(uint8_t group, uint8_t mdsId, void* data, uint32_t length, cmidi2_mds_handler sendUMP, void* context)
{
    int32_t numChunks = cmidi2_ump_mds_get_num_chunks(length);
    for (int c = 0; c < numChunks; c++) {
        int32_t maxChunkSize = 14 * 65535;
        int32_t chunkSize = c + 1 == numChunks ? length % maxChunkSize : maxChunkSize;
        int32_t numPayloads = cmidi2_ump_mds_get_num_payloads(chunkSize);
        for (int p = 0; p < numPayloads; p++) {
            uint64_t result1, result2;
            size_t offset = 14 * (65536 * c + p);
            cmidi2_ump_mds_get_payload_of(group, mdsId, chunkSize, (uint8_t*) data + offset, &result1, &result2);
            sendUMP(result1, result2, c, p, context);
        }
    }
}


// Strongly-typed(?) UMP.
// I kind of think those getters are overkill, so I would collect almost use `cmidi2_ump_get_xxx()`
// as those strongly typed functions, so that those who don't want them can safely ignore them.

typedef struct cmidi2_ump_tag {
    // type and group
    uint8_t type_byte;
    // status and 
    uint8_t status_byte;
} cmidi2_ump;

#ifdef _PACKET_
#define _BACKUP_PACKET_ _PACKET_
#endif
#define _PACKET_(index) ((uint8_t*) ump)[index]

static inline uint8_t cmidi2_ump_get_message_type(cmidi2_ump* ump) {
    return (ump->type_byte & 0xF0) >> 4;
}

static inline uint8_t cmidi2_ump_get_group(cmidi2_ump* ump) {
    return ump->type_byte & 0xF;
}

static inline uint8_t cmidi2_ump_get_status_code(cmidi2_ump* ump) {
    return ump->status_byte & 0xF0;
}

static inline uint8_t cmidi2_ump_get_channel(cmidi2_ump* ump) {
    return ump->status_byte & 0xF;
}

static inline uint32_t cmidi2_ump_get_32_to_64(cmidi2_ump* ump) {
    uint8_t* p8 = (uint8_t*) ump;
    return (uint32_t) (p8[4] << 24) + (p8[5] << 16) + (p8[6] << 8) + p8[7];
}

static inline uint16_t cmidi2_ump_get_jr_clock_time(cmidi2_ump* ump) {
    return (_PACKET_(2) << 8) + _PACKET_(3);
}
static inline uint16_t cmidi2_ump_get_jr_timestamp_timestamp(cmidi2_ump* ump) {
    return (_PACKET_(2) << 8) + _PACKET_(3);
}

static inline uint8_t cmidi2_ump_get_system_message_byte2(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint8_t cmidi2_ump_get_system_message_byte3(cmidi2_ump* ump) {
    return _PACKET_(3);
}

static inline uint8_t cmidi2_ump_get_midi1_byte2(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint8_t cmidi2_ump_get_midi1_byte3(cmidi2_ump* ump) {
    return _PACKET_(3);
}

static inline uint8_t cmidi2_ump_get_midi1_note_note(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint8_t cmidi2_ump_get_midi1_note_velocity(cmidi2_ump* ump) {
    return _PACKET_(3);
}
static inline uint8_t cmidi2_ump_get_midi1_paf_note(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint8_t cmidi2_ump_get_midi1_paf_data(cmidi2_ump* ump) {
    return _PACKET_(3);
}
static inline uint8_t cmidi2_ump_get_midi1_cc_index(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint8_t cmidi2_ump_get_midi1_cc_data(cmidi2_ump* ump) {
    return _PACKET_(3);
}
static inline uint8_t cmidi2_ump_get_midi1_program_program(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint8_t cmidi2_ump_get_midi1_caf_data(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint16_t cmidi2_ump_get_midi1_pitch_bend_data(cmidi2_ump* ump) {
    return _PACKET_(2) + _PACKET_(3) * 0x80;
}

static inline uint8_t cmidi2_ump_get_sysex7_num_bytes(cmidi2_ump* ump) {
    return cmidi2_ump_get_channel(ump); // same bits
}

static inline uint8_t cmidi2_ump_get_midi2_note_note(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint8_t cmidi2_ump_get_midi2_note_attribute_type(cmidi2_ump* ump) {
    return _PACKET_(3);
}
static inline uint16_t cmidi2_ump_get_midi2_note_velocity(cmidi2_ump* ump) {
    return (_PACKET_(4) << 8) + _PACKET_(5);
}
static inline uint16_t cmidi2_ump_get_midi2_note_attribute_data(cmidi2_ump* ump) {
    return (_PACKET_(6) << 8) + _PACKET_(7);
}
static inline uint8_t cmidi2_ump_get_midi2_paf_note(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint32_t cmidi2_ump_get_midi2_paf_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
static inline uint8_t cmidi2_ump_get_midi2_pnrcc_note(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint8_t cmidi2_ump_get_midi2_pnrcc_index(cmidi2_ump* ump) {
    return _PACKET_(3);
}
static inline uint32_t cmidi2_ump_get_midi2_pnrcc_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
static inline uint8_t cmidi2_ump_get_midi2_pnacc_note(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint8_t cmidi2_ump_get_midi2_pnacc_index(cmidi2_ump* ump) {
    return _PACKET_(3);
}
static inline uint32_t cmidi2_ump_get_midi2_pnacc_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
static inline uint32_t cmidi2_ump_get_midi2_pn_management_note(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint32_t cmidi2_ump_get_midi2_pn_management_options(cmidi2_ump* ump) {
    return _PACKET_(3);
}
static inline uint8_t cmidi2_ump_get_midi2_cc_index(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint32_t cmidi2_ump_get_midi2_cc_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
// absolute RPN or relative RPN
static inline uint8_t cmidi2_ump_get_midi2_rpn_msb(cmidi2_ump* ump) {
    return _PACKET_(2);
}
// absolute RPN or relative RPN
static inline uint8_t cmidi2_ump_get_midi2_rpn_lsb(cmidi2_ump* ump) {
    return _PACKET_(3);
}
// absolute RPN or relative RPN
static inline uint32_t cmidi2_ump_get_midi2_rpn_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
// absolute NRPN or relative NRPN
static inline uint8_t cmidi2_ump_get_midi2_nrpn_msb(cmidi2_ump* ump) {
    return _PACKET_(2);
}
// absolute NRPN or relative NRPN
static inline uint8_t cmidi2_ump_get_midi2_nrpn_lsb(cmidi2_ump* ump) {
    return _PACKET_(3);
}
// absolute NRPN or relative NRPN
static inline uint32_t cmidi2_ump_get_midi2_nrpn_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
static inline uint8_t cmidi2_ump_get_midi2_program_options(cmidi2_ump* ump) {
    return _PACKET_(3);
}
static inline uint8_t cmidi2_ump_get_midi2_program_program(cmidi2_ump* ump) {
    return _PACKET_(4);
}
static inline uint8_t cmidi2_ump_get_midi2_program_bank_msb(cmidi2_ump* ump) {
    return _PACKET_(6);
}
static inline uint8_t cmidi2_ump_get_midi2_program_bank_lsb(cmidi2_ump* ump) {
    return _PACKET_(7);
}
static inline uint32_t cmidi2_ump_get_midi2_caf_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
// either per-note or channel
static inline uint32_t cmidi2_ump_get_midi2_pitch_bend_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
static inline uint8_t cmidi2_ump_get_midi2_pn_pitch_bend_note(cmidi2_ump* ump) {
    return _PACKET_(2);
}

static inline uint8_t cmidi2_ump_get_sysex8_num_bytes(cmidi2_ump* ump) {
    return cmidi2_ump_get_channel(ump); // same bits
}
static inline uint8_t cmidi2_ump_get_sysex8_stream_id(cmidi2_ump* ump) {
    return _PACKET_(2);
}
static inline uint8_t cmidi2_ump_get_mds_mds_id(cmidi2_ump* ump) {
    return cmidi2_ump_get_channel(ump); // same bits
}
static inline uint16_t cmidi2_ump_get_mds_num_chunk_bytes(cmidi2_ump* ump) {
    return (_PACKET_(2) << 8) + _PACKET_(3);
}
static inline uint16_t cmidi2_ump_get_mds_num_chunks(cmidi2_ump* ump) {
    return (_PACKET_(4) << 8) + _PACKET_(5);
}
static inline uint16_t cmidi2_ump_get_mds_chunk_index(cmidi2_ump* ump) {
    return (_PACKET_(6) << 8) + _PACKET_(7);
}
static inline uint16_t cmidi2_ump_get_mds_manufacturer_id(cmidi2_ump* ump) {
    return (_PACKET_(8) << 8) + _PACKET_(9);
}
static inline uint16_t cmidi2_ump_get_mds_device_id(cmidi2_ump* ump) {
    return (_PACKET_(10) << 8) + _PACKET_(11);
}
static inline uint16_t cmidi2_ump_get_mds_sub_id_1(cmidi2_ump* ump) {
    return (_PACKET_(12) << 8) + _PACKET_(13);
}
static inline uint16_t cmidi2_ump_get_mds_sub_id_2(cmidi2_ump* ump) {
    return (_PACKET_(14) << 8) + _PACKET_(15);
}

#ifdef _BACKUP_PACKET_
#define _PACKET_ _BACKUP_PACKET_
#undef _BACKUP_PACKET_
#endif

// sequence iterator

/* byte stream splitter */

static inline void* cmidi2_ump_sequence_next(void* ptr) {
    return (uint8_t*) ptr + cmidi2_ump_get_num_bytes(cmidi2_ump_read_uint32_bytes(ptr));
}

// similar to LV2_ATOM Utilities API...
#define CMIDI2_UMP_SEQUENCE_FOREACH(ptr, numBytes, iter) \
    for (uint8_t* (iter) = (uint8_t*) ptr; \
        (iter) < ((uint8_t*) ptr) + numBytes; \
        (iter) = (uint8_t*) cmidi2_ump_sequence_next(iter))

#ifdef __cplusplus
}
#endif
