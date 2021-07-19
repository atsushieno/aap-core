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

enum cmidi2_cc {
    CMIDI2_CC_BANK_SELECT = 0x00,
    CMIDI2_CC_MODULATION = 0x01,
    CMIDI2_CC_BREATH = 0x02,
    CMIDI2_CC_FOOT = 0x04,
    CMIDI2_CC_PORTAMENTO_TIME = 0x05,
    CMIDI2_CC_DTE_MSB = 0x06,
    CMIDI2_CC_VOLUME = 0x07,
    CMIDI2_CC_BALANCE = 0x08,
    CMIDI2_CC_PAN = 0x0A,
    CMIDI2_CC_EXPRESSION = 0x0B,
    CMIDI2_CC_EFFECT_CONTROL_1 = 0x0C,
    CMIDI2_CC_EFFECT_CONTROL_2 = 0x0D,
    CMIDI2_CC_GENERAL_1 = 0x10,
    CMIDI2_CC_GENERAL_2 = 0x11,
    CMIDI2_CC_GENERAL_3 = 0x12,
    CMIDI2_CC_GENERAL_4 = 0x13,
    CMIDI2_CC_BANK_SELECT_LSB = 0x20,
    CMIDI2_CC_MODULATION_LSB = 0x21,
    CMIDI2_CC_BREATH_LSB = 0x22,
    CMIDI2_CC_FOOT_LSB = 0x24,
    CMIDI2_CC_PORTAMENTO_TIME_LSB = 0x25,
    CMIDI2_CC_DTE_LSB = 0x26,
    CMIDI2_CC_VOLUME_LSB = 0x27,
    CMIDI2_CC_BALANCE_LSB = 0x28,
    CMIDI2_CC_PAN_LSB = 0x2A,
    CMIDI2_CC_EXPRESSION_LSB = 0x2B,
    CMIDI2_CC_EFFECT1_LSB = 0x2C,
    CMIDI2_CC_EFFECT2_LSB = 0x2D,
    CMIDI2_CC_GENERAL_1_LSB = 0x30,
    CMIDI2_CC_GENERAL_2_LSB = 0x31,
    CMIDI2_CC_GENERAL_3_LSB = 0x32,
    CMIDI2_CC_GENERAL_4_LSB = 0x33,
    CMIDI2_CC_HOLD = 0x40,
    CMIDI2_CC_PORTAMENTO_SWITCH = 0x41,
    CMIDI2_CC_SOSTENUTO = 0x42,
    CMIDI2_CC_SOFT_PEDAL = 0x43,
    CMIDI2_CC_LEGATO = 0x44,
    CMIDI2_CC_HOLD_2 = 0x45,
    CMIDI2_CC_SOUND_CONTROLLER_1 = 0x46,
    CMIDI2_CC_SOUND_CONTROLLER_2 = 0x47,
    CMIDI2_CC_SOUND_CONTROLLER_3 = 0x48,
    CMIDI2_CC_SOUND_CONTROLLER_4 = 0x49,
    CMIDI2_CC_SOUND_CONTROLLER_5 = 0x4A,
    CMIDI2_CC_SOUND_CONTROLLER_6 = 0x4B,
    CMIDI2_CC_SOUND_CONTROLLER_7 = 0x4C,
    CMIDI2_CC_SOUND_CONTROLLER_8 = 0x4D,
    CMIDI2_CC_SOUND_CONTROLLER_9 = 0x4E,
    CMIDI2_CC_SOUND_CONTROLLER_10 = 0x4F,
    CMIDI2_CC_GENERAL_5 = 0x50,
    CMIDI2_CC_GENERAL_6 = 0x51,
    CMIDI2_CC_GENERAL_7 = 0x52,
    CMIDI2_CC_GENERAL_8 = 0x53,
    CMIDI2_CC_PORTAMENTO_CONTROL = 0x54,
    CMIDI2_CC_RSD = 0x5B,
    CMIDI2_CC_EFFECT_1 = 0x5B,
    CMIDI2_CC_TREMOLO = 0x5C,
    CMIDI2_CC_EFFECT_2 = 0x5C,
    CMIDI2_CC_CSD = 0x5D,
    CMIDI2_CC_EFFECT_3 = 0x5D,
    CMIDI2_CC_CELESTE = 0x5E,
    CMIDI2_CC_EFFECT_4 = 0x5E,
    CMIDI2_CC_PHASER = 0x5F,
    CMIDI2_CC_EFFECT_5 = 0x5F,
    CMIDI2_CC_DTE_INCREMENT = 0x60,
    CMIDI2_CC_DTE_DECREMENT = 0x61,
    CMIDI2_CC_NRPN_LSB = 0x62,
    CMIDI2_CC_NRPN_MSB = 0x63,
    CMIDI2_CC_RPN_LSB = 0x64,
    CMIDI2_CC_RPN_MSB = 0x65,
    // CHANNEL MODE MESSAGES
    CMIDI2_CC_ALL_SOUND_OFF = 0x78,
    CMIDI2_CC_RESET_ALL_CONTROLLERS = 0x79,
    CMIDI2_CC_LOCAL_CONTROL = 0x7A,
    CMIDI2_CC_ALL_NOTES_OFF = 0x7B,
    CMIDI2_CC_OMNI_MODE_OFF = 0x7C,
    CMIDI2_CC_OMNI_MODE_ON = 0x7D,
    CMIDI2_CC_POLY_MODE_ON_OFF = 0x7E,
    CMIDI2_CC_POLY_MODE_ON = 0x7F,
};

enum cmidi2_rpn {
    CMIDI2_RPN_PITCH_BEND_SENSITIVITY = 0,
    CMIDI2_RPN_FINE_TUNING = 1,
    CMIDI2_RPN_COARSE_TUNING = 2,
    CMIDI2_RPN_TUNING_PROGRAM = 3,
    CMIDI2_RPN_TUNING_BANK_SELECT = 4,
    CMIDI2_RPN_MODULATION_DEPTH = 5,
};

enum cmidi2_meta_event_type {
    CMIDI2_META_SEQUENCE_NUMBER = 0X00,
    CMIDI2_META_TEXT = 0X01,
    CMIDI2_META_COPYRIGHT = 0X02,
    CMIDI2_META_TRACK_NAME = 0X03,
    CMIDI2_META_INSTRUMENT_NAME = 0X04,
    CMIDI2_META_LYRIC = 0X05,
    CMIDI2_META_MARKER = 0X06,
    CMIDI2_META_CUE = 0X07,
    CMIDI2_META_CHANNEL_PREFIX = 0X20,
    CMIDI2_META_END_OF_TRACK = 0X2F,
    CMIDI2_META_TEMPO = 0X51,
    CMIDI2_META_SMPTE_OFFSET = 0X54,
    CMIDI2_META_TIME_SIGNATURE = 0X58,
    CMIDI2_META_KEY_SIGNATURE = 0X59,
    CMIDI2_META_SEQUENCER_SPECIFIC = 0X7F,
};

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

typedef uint32_t cmidi2_ump;

static inline uint8_t cmidi2_ump_get_byte_at(cmidi2_ump *ump, uint8_t at) {
    ump += at / 4;
    switch (at % 4) {
    case 0: return (*ump & 0xFF000000) >> 24;
    case 1: return (*ump & 0xFF0000) >> 16;
    case 2: return (*ump & 0xFF00) >> 8;
    case 3: return *ump & 0xFF;
    }
    return 0; // This is unexpected.
}

static inline uint8_t cmidi2_ump_get_message_type(cmidi2_ump* ump) {
    return *ump >> 28;
}

static inline uint8_t cmidi2_ump_get_group(cmidi2_ump* ump) {
    return (*ump >> 24) & 0xF;
}

static inline uint8_t cmidi2_ump_get_status_code(cmidi2_ump* ump) {
    return (*ump >> 16) & 0xF0;
}

static inline uint8_t cmidi2_ump_get_channel(cmidi2_ump* ump) {
    return (*ump >> 16) & 0xF;
}

static inline uint32_t cmidi2_ump_get_32_to_64(cmidi2_ump* ump) {
    return *(ump + 1);
}

static inline uint16_t cmidi2_ump_get_jr_clock_time(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 2) << 8) + cmidi2_ump_get_byte_at(ump, 3);
}
static inline uint16_t cmidi2_ump_get_jr_timestamp_timestamp(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 2) << 8) + cmidi2_ump_get_byte_at(ump, 3);
}

static inline uint8_t cmidi2_ump_get_system_message_byte2(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint8_t cmidi2_ump_get_system_message_byte3(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}

static inline uint8_t cmidi2_ump_get_midi1_byte2(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint8_t cmidi2_ump_get_midi1_byte3(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}

static inline uint8_t cmidi2_ump_get_midi1_note_note(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint8_t cmidi2_ump_get_midi1_note_velocity(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}
static inline uint8_t cmidi2_ump_get_midi1_paf_note(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint8_t cmidi2_ump_get_midi1_paf_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}
static inline uint8_t cmidi2_ump_get_midi1_cc_index(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint8_t cmidi2_ump_get_midi1_cc_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}
static inline uint8_t cmidi2_ump_get_midi1_program_program(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint8_t cmidi2_ump_get_midi1_caf_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint16_t cmidi2_ump_get_midi1_pitch_bend_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2) + cmidi2_ump_get_byte_at(ump, 3) * 0x80;
}

static inline uint8_t cmidi2_ump_get_sysex7_num_bytes(cmidi2_ump* ump) {
    return cmidi2_ump_get_channel(ump); // same bits
}

static inline uint8_t cmidi2_ump_get_midi2_note_note(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint8_t cmidi2_ump_get_midi2_note_attribute_type(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}
static inline uint16_t cmidi2_ump_get_midi2_note_velocity(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 4) << 8) + (cmidi2_ump_get_byte_at(ump, 5));
}
static inline uint16_t cmidi2_ump_get_midi2_note_attribute_data(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 6) << 8) + (cmidi2_ump_get_byte_at(ump, 7));
}
static inline uint8_t cmidi2_ump_get_midi2_paf_note(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint32_t cmidi2_ump_get_midi2_paf_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
static inline uint8_t cmidi2_ump_get_midi2_pnrcc_note(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint8_t cmidi2_ump_get_midi2_pnrcc_index(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}
static inline uint32_t cmidi2_ump_get_midi2_pnrcc_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
static inline uint8_t cmidi2_ump_get_midi2_pnacc_note(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint8_t cmidi2_ump_get_midi2_pnacc_index(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}
static inline uint32_t cmidi2_ump_get_midi2_pnacc_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
static inline uint32_t cmidi2_ump_get_midi2_pn_management_note(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint32_t cmidi2_ump_get_midi2_pn_management_options(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}
static inline uint8_t cmidi2_ump_get_midi2_cc_index(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint32_t cmidi2_ump_get_midi2_cc_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
// absolute RPN or relative RPN
static inline uint8_t cmidi2_ump_get_midi2_rpn_msb(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
// absolute RPN or relative RPN
static inline uint8_t cmidi2_ump_get_midi2_rpn_lsb(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}
// absolute RPN or relative RPN
static inline uint32_t cmidi2_ump_get_midi2_rpn_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
// absolute NRPN or relative NRPN
static inline uint8_t cmidi2_ump_get_midi2_nrpn_msb(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
// absolute NRPN or relative NRPN
static inline uint8_t cmidi2_ump_get_midi2_nrpn_lsb(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}
// absolute NRPN or relative NRPN
static inline uint32_t cmidi2_ump_get_midi2_nrpn_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
static inline uint8_t cmidi2_ump_get_midi2_program_options(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 3);
}
static inline uint8_t cmidi2_ump_get_midi2_program_program(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 4);
}
static inline uint8_t cmidi2_ump_get_midi2_program_bank_msb(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 6));
}
static inline uint8_t cmidi2_ump_get_midi2_program_bank_lsb(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 7));
}
static inline uint32_t cmidi2_ump_get_midi2_caf_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
// either per-note or channel
static inline uint32_t cmidi2_ump_get_midi2_pitch_bend_data(cmidi2_ump* ump) {
    return cmidi2_ump_get_32_to_64(ump);
}
static inline uint8_t cmidi2_ump_get_midi2_pn_pitch_bend_note(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}

static inline uint8_t cmidi2_ump_get_sysex8_num_bytes(cmidi2_ump* ump) {
    return cmidi2_ump_get_channel(ump); // same bits
}
static inline uint8_t cmidi2_ump_get_sysex8_stream_id(cmidi2_ump* ump) {
    return cmidi2_ump_get_byte_at(ump, 2);
}
static inline uint8_t cmidi2_ump_get_mds_mds_id(cmidi2_ump* ump) {
    return cmidi2_ump_get_channel(ump); // same bits
}
static inline uint16_t cmidi2_ump_get_mds_num_chunk_bytes(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 2) << 8) + cmidi2_ump_get_byte_at(ump, 3);
}
static inline uint16_t cmidi2_ump_get_mds_num_chunks(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 4) << 8) + cmidi2_ump_get_byte_at(ump, 5);
}
static inline uint16_t cmidi2_ump_get_mds_chunk_index(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 6) << 8) + cmidi2_ump_get_byte_at(ump, 7);
}
static inline uint16_t cmidi2_ump_get_mds_manufacturer_id(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 8) << 8) + cmidi2_ump_get_byte_at(ump, 9);
}
static inline uint16_t cmidi2_ump_get_mds_device_id(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 10) << 8) + cmidi2_ump_get_byte_at(ump, 11);
}
static inline uint16_t cmidi2_ump_get_mds_sub_id_1(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 12) << 8) + cmidi2_ump_get_byte_at(ump, 13);
}
static inline uint16_t cmidi2_ump_get_mds_sub_id_2(cmidi2_ump* ump) {
    return (cmidi2_ump_get_byte_at(ump, 14) << 8) + cmidi2_ump_get_byte_at(ump, 15);
}

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


// MIDI CI support.

#define CMIDI2_CI_SUB_ID 0xD
#define CMIDI2_CI_SUB_ID_2_DISCOVERY_INQUIRY 0x70
#define CMIDI2_CI_SUB_ID_2_DISCOVERY_REPLY 0x71
#define CMIDI2_CI_SUB_ID_2_INVALIDATE_MUID 0x7E
#define CMIDI2_CI_SUB_ID_2_NAK 0x7F
#define CMIDI2_CI_SUB_ID_2_PROTOCOL_NEGOTIATION_INQUIRY 0x10
#define CMIDI2_CI_SUB_ID_2_PROTOCOL_NEGOTIATION_REPLY 0x11
#define CMIDI2_CI_SUB_ID_2_SET_NEW_PROTOCOL 0x12
#define CMIDI2_CI_SUB_ID_2_TEST_NEW_PROTOCOL_I2R 0x13
#define CMIDI2_CI_SUB_ID_2_TEST_NEW_PROTOCOL_R2I 0x14
#define CMIDI2_CI_SUB_ID_2_CONFIRM_NEW_PROTOCOL_ESTABLISHED 0x15
#define CMIDI2_CI_SUB_ID_2_PROFILE_INQUIRY 0x20
#define CMIDI2_CI_SUB_ID_2_PROFILE_INQUIRY_REPLY 0x21
#define CMIDI2_CI_SUB_ID_2_SET_PROFILE_ON 0x22
#define CMIDI2_CI_SUB_ID_2_SET_PROFILE_OFF 0x23
#define CMIDI2_CI_SUB_ID_2_PROFILE_ENABLED_REPORT 0x24
#define CMIDI2_CI_SUB_ID_2_PROFILE_DISABLED_REPORT 0x25
#define CMIDI2_CI_SUB_ID_2_PROFILE_SPECIFIC_DATA 0x2F
#define CMIDI2_CI_SUB_ID_2_PROPERTY_CAPABILITIES_INQUIRY 0x30
#define CMIDI2_CI_SUB_ID_2_PROPERTY_CAPABILITIES_REPLY 0x31
#define CMIDI2_CI_SUB_ID_2_PROPERTY_HAS_DATA_INQUIRY 0x32
#define CMIDI2_CI_SUB_ID_2_PROPERTY_HAS_DATA_REPLY 0x33
#define CMIDI2_CI_SUB_ID_2_PROPERTY_GET_DATA_INQUIRY 0x34
#define CMIDI2_CI_SUB_ID_2_PROPERTY_GET_DATA_REPLY 0x35
#define CMIDI2_CI_SUB_ID_2_PROPERTY_SET_DATA_INQUIRY 0x36
#define CMIDI2_CI_SUB_ID_2_PROPERTY_SET_DATA_REPLY 0x37
#define CMIDI2_CI_SUB_ID_2_PROPERTY_SUBSCRIBE 0x38
#define CMIDI2_CI_SUB_ID_2_PROPERTY_SUBSCRIBE_REPLY 0x39
#define CMIDI2_CI_SUB_ID_2_PROPERTY_NOTIFY 0x3F

typedef struct {
    uint8_t type;
    uint8_t version;
    uint8_t extensions;
    uint8_t reserved1;
    uint8_t reserved2;
} cmidi2_ci_protocol_type_info;

typedef struct {
    uint8_t fixed_7e; // 0x7E
    uint8_t bank;
    uint8_t number;
    uint8_t version;
    uint8_t level;
} cmidi2_profile_id;

// Assumes the input value is already 7-bit encoded if required.
static inline void cmidi2_ci_direct_uint16_at(uint8_t* buf, uint16_t v) {
    buf[0] = v & 0xFF;
    buf[1] = (v >> 8) & 0xFF;
}

// Assumes the input value is already 7-bit encoded if required.
static inline void cmidi2_ci_direct_uint32_at(uint8_t* buf, uint32_t v) {
    buf[0] = v & 0xFF;
    buf[1] = (v >> 8) & 0xFF;
    buf[2] = (v >> 16) & 0xFF;
    buf[3] = (v >> 24) & 0xFF;
}

static inline void cmidi2_ci_7bit_int14_at(uint8_t* buf, uint16_t v) {
    buf[0] = v & 0x7F;
    buf[1] = (v >> 7) & 0x7F;
}

static inline void cmidi2_ci_7bit_int21_at(uint8_t* buf, uint32_t v) {
    buf[0] = v & 0x7F;
    buf[1] = (v >> 7) & 0x7F;
    buf[2] = (v >> 14) & 0x7F;
}

static inline void cmidi2_ci_7bit_int28_at(uint8_t* buf, uint32_t v) {
    buf[0] = v & 0x7F;
    buf[1] = (v >> 7) & 0x7F;
    buf[2] = (v >> 14) & 0x7F;
    buf[3] = (v >> 21) & 0x7F;
}

static inline void cmidi2_ci_message_common(uint8_t* buf,
        uint8_t destination, uint8_t sysexSubId2, uint8_t versionAndFormat, uint32_t sourceMUID, uint32_t destinationMUID) {
    buf[0] = 0x7E;
    buf[1] = destination;
    buf[2] = 0xD;
    buf[3] = sysexSubId2;
    buf[4] = versionAndFormat;
    cmidi2_ci_direct_uint32_at(buf + 5, sourceMUID);
    cmidi2_ci_direct_uint32_at(buf + 9, destinationMUID);
}

// Discovery

static inline void cmidi2_ci_discovery_common(uint8_t* buf, uint8_t sysexSubId2,
    uint8_t versionAndFormat, uint32_t sourceMUID, uint32_t destinationMUID,
    uint32_t deviceManufacturer3Bytes, uint16_t deviceFamily, uint16_t deviceFamilyModelNumber,
    uint32_t softwareRevisionLevel, uint8_t ciCategorySupported, uint32_t receivableMaxSysExSize) {
    cmidi2_ci_message_common(buf, 0x7F, sysexSubId2, versionAndFormat, sourceMUID, destinationMUID);
    cmidi2_ci_direct_uint32_at(buf + 13, deviceManufacturer3Bytes); // the last byte is extraneous, but will be overwritten next.
    cmidi2_ci_direct_uint16_at(buf + 16, deviceFamily);
    cmidi2_ci_direct_uint16_at(buf + 18, deviceFamilyModelNumber);
    // LAMESPEC: Software Revision Level does not mention in which endianness this field is stored.
    cmidi2_ci_direct_uint32_at(buf + 20, softwareRevisionLevel);
    buf[24] = ciCategorySupported;
    cmidi2_ci_direct_uint32_at(buf + 25, receivableMaxSysExSize);
}

static inline void cmidi2_ci_discovery(uint8_t* buf,
    uint8_t versionAndFormat, uint32_t sourceMUID,
    uint32_t deviceManufacturer, uint16_t deviceFamily, uint16_t deviceFamilyModelNumber,
    uint32_t softwareRevisionLevel, uint8_t ciCategorySupported, uint32_t receivableMaxSysExSize) {
    cmidi2_ci_discovery_common(buf, CMIDI2_CI_SUB_ID_2_DISCOVERY_INQUIRY,
        versionAndFormat, sourceMUID, 0x7F7F7F7F,
        deviceManufacturer, deviceFamily, deviceFamilyModelNumber,
        softwareRevisionLevel, ciCategorySupported, receivableMaxSysExSize);
}

static inline void cmidi2_ci_discovery_reply(uint8_t* buf,
    uint8_t versionAndFormat, uint32_t sourceMUID, uint32_t destinationMUID,
    uint32_t deviceManufacturer, uint16_t deviceFamily, uint16_t deviceFamilyModelNumber,
    uint32_t softwareRevisionLevel, uint8_t ciCategorySupported, uint32_t receivableMaxSysExSize) {
    cmidi2_ci_discovery_common(buf, CMIDI2_CI_SUB_ID_2_DISCOVERY_REPLY,
        versionAndFormat, sourceMUID, destinationMUID,
        deviceManufacturer, deviceFamily, deviceFamilyModelNumber,
        softwareRevisionLevel, ciCategorySupported, receivableMaxSysExSize);
}

static inline void cmidi2_ci_discovery_invalidate_muid(uint8_t* buf,
    uint8_t versionAndFormat, uint32_t sourceMUID, uint32_t targetMUID) {
    cmidi2_ci_message_common(buf, 0x7F, 0x7E, versionAndFormat, sourceMUID, 0x7F7F7F7F);
    cmidi2_ci_direct_uint32_at(buf + 13, targetMUID);
}

static inline void cmidi2_ci_discovery_nak(uint8_t* buf, uint8_t deviceId,
    uint8_t versionAndFormat, uint32_t sourceMUID, uint32_t destinationMUID) {
    cmidi2_ci_message_common(buf, deviceId, 0x7F, versionAndFormat, sourceMUID, destinationMUID);
}

// Protocol Negotiation

static inline void cmidi2_ci_protocol_info(uint8_t* buf, cmidi2_ci_protocol_type_info info) {
    buf[0] = info.type;
    buf[1] = info.version;
    buf[2] = info.extensions;
    buf[3] = info.reserved1;
    buf[4] = info.reserved2;
}

static inline void cmidi2_ci_protocols(uint8_t* buf, uint8_t numSupportedProtocols, cmidi2_ci_protocol_type_info* protocolTypes) {
    buf[0] = numSupportedProtocols;
    for (int i = 0; i < numSupportedProtocols; i++)
        cmidi2_ci_protocol_info(buf + 1 + i * 5, protocolTypes[i]);
}

static inline void cmidi2_ci_protocol_negotiation(uint8_t* buf, bool isReply,
    uint32_t sourceMUID, uint32_t destinationMUID,
    uint8_t authorityLevel,
    uint8_t numSupportedProtocols, cmidi2_ci_protocol_type_info* protocolTypes) {
    cmidi2_ci_message_common(buf, 0x7F,
        isReply ? CMIDI2_CI_SUB_ID_2_PROTOCOL_NEGOTIATION_REPLY : CMIDI2_CI_SUB_ID_2_PROTOCOL_NEGOTIATION_INQUIRY,
        1, sourceMUID, destinationMUID);
    buf[13] = authorityLevel;
    cmidi2_ci_protocols(buf + 14, numSupportedProtocols, protocolTypes);
}

static inline void cmidi2_ci_protocol_set(uint8_t* buf,
    uint32_t sourceMUID, uint32_t destinationMUID,
    uint8_t authorityLevel, cmidi2_ci_protocol_type_info newProtocolType) {
    cmidi2_ci_message_common(buf, 0x7F,
        CMIDI2_CI_SUB_ID_2_SET_NEW_PROTOCOL,
        1, sourceMUID, destinationMUID);
    buf[13] = authorityLevel;
    cmidi2_ci_protocol_info(buf + 14, newProtocolType);
}

static inline void cmidi2_ci_protocol_test(uint8_t* buf,
    bool isInitiatorToResponder,
    uint32_t sourceMUID, uint32_t destinationMUID,
    uint8_t authorityLevel, uint8_t* testData48Bytes) {
    cmidi2_ci_message_common(buf, 0x7F,
        isInitiatorToResponder ? CMIDI2_CI_SUB_ID_2_TEST_NEW_PROTOCOL_I2R : CMIDI2_CI_SUB_ID_2_TEST_NEW_PROTOCOL_R2I,
        1, sourceMUID, destinationMUID);
    buf[13] = authorityLevel;
    memcpy(buf + 14, testData48Bytes, 48);
}

static inline void cmidi2_ci_protocol_confirm_established(uint8_t* buf,
    uint32_t sourceMUID, uint32_t destinationMUID,
    uint8_t authorityLevel) {
    cmidi2_ci_message_common(buf, 0x7F,
        CMIDI2_CI_SUB_ID_2_CONFIRM_NEW_PROTOCOL_ESTABLISHED,
        1, sourceMUID, destinationMUID);
    buf[13] = authorityLevel;
}

// Profile Configuration

static inline void cmidi2_ci_profile(uint8_t* buf, cmidi2_profile_id info) {
    buf[0] = info.fixed_7e;
    buf[1] = info.bank;
    buf[2] = info.number;
    buf[3] = info.version;
    buf[4] = info.level;
}

static inline void cmidi2_ci_profile_inquiry(uint8_t* buf, uint8_t source,
    uint32_t sourceMUID, uint32_t destinationMUID) {
    cmidi2_ci_message_common(buf, source,
        CMIDI2_CI_SUB_ID_2_PROFILE_INQUIRY,
        1, sourceMUID, destinationMUID);
}

static inline void cmidi2_ci_profile_inquiry_reply(uint8_t* buf, uint8_t source,
    uint32_t sourceMUID, uint32_t destinationMUID,
    uint8_t numEnabledProfiles, cmidi2_profile_id* enabledProfiles,
    uint8_t numDisabledProfiles, cmidi2_profile_id* disabledProfiles) {
    cmidi2_ci_message_common(buf, source,
        CMIDI2_CI_SUB_ID_2_PROFILE_INQUIRY_REPLY,
        1, sourceMUID, destinationMUID);
    buf[13] = numEnabledProfiles;
    for (int i = 0; i < numEnabledProfiles; i++)
        cmidi2_ci_profile(buf + 14 + i * 5, enabledProfiles[i]);
    uint32_t pos = 14 + numEnabledProfiles * 5;
    buf[pos++] = numDisabledProfiles;
    for (int i = 0; i < numDisabledProfiles; i++)
        cmidi2_ci_profile(buf + pos + i * 5, disabledProfiles[i]);
}

static inline void cmidi2_ci_profile_set(uint8_t* buf, uint8_t destination, bool turnOn,
    uint32_t sourceMUID, uint32_t destinationMUID, cmidi2_profile_id profile) {
    cmidi2_ci_message_common(buf, destination,
        turnOn ? CMIDI2_CI_SUB_ID_2_SET_PROFILE_ON : CMIDI2_CI_SUB_ID_2_SET_PROFILE_OFF,
        1, sourceMUID, destinationMUID);
    cmidi2_ci_profile(buf + 13, profile);
}

static inline void cmidi2_ci_profile_report(uint8_t* buf, uint8_t source, bool isEnabledReport,
    uint32_t sourceMUID, cmidi2_profile_id profile) {
    cmidi2_ci_message_common(buf, source,
        isEnabledReport ? CMIDI2_CI_SUB_ID_2_PROFILE_ENABLED_REPORT : CMIDI2_CI_SUB_ID_2_PROFILE_DISABLED_REPORT,
        1, sourceMUID, 0x7F7F7F7F);
    cmidi2_ci_profile(buf + 13, profile);
}

static inline void cmidi2_ci_profile_specific_data(uint8_t* buf, uint8_t source,
    uint32_t sourceMUID, uint32_t destinationMUID, cmidi2_profile_id profile, uint32_t dataSize, void* data) {
    cmidi2_ci_message_common(buf, source,
        CMIDI2_CI_SUB_ID_2_PROFILE_SPECIFIC_DATA,
        1, sourceMUID, destinationMUID);
    cmidi2_ci_profile(buf + 13, profile);
    cmidi2_ci_direct_uint32_at(buf + 18, dataSize);
    memcpy(buf + 22, data, dataSize);
}

// Property Exchange

static inline void cmidi2_ci_property_get_capabilities(uint8_t* buf, uint8_t destination, bool isReply,
    uint32_t sourceMUID, uint32_t destinationMUID, uint8_t maxSupportedRequests) {
    cmidi2_ci_message_common(buf, destination,
        isReply ? CMIDI2_CI_SUB_ID_2_PROPERTY_CAPABILITIES_REPLY : CMIDI2_CI_SUB_ID_2_PROPERTY_CAPABILITIES_INQUIRY,
        1, sourceMUID, destinationMUID);
    buf[13] = maxSupportedRequests;
}

// common to all of: has data & reply, get data & reply, set data & reply, subscribe & reply, notify
static inline void cmidi2_ci_property_common(uint8_t* buf, uint8_t destination, uint8_t messageTypeSubId2,
    uint32_t sourceMUID, uint32_t destinationMUID,
    uint8_t requestId, uint16_t headerSize, void* header,
    uint16_t numChunks, uint16_t chunkIndex, uint16_t dataSize, void* data) {
    cmidi2_ci_message_common(buf, destination, messageTypeSubId2, 1, sourceMUID, destinationMUID);
    buf[13] = requestId;
    cmidi2_ci_direct_uint16_at(buf + 14, headerSize);
    memcpy(buf + 16, header, headerSize);
    cmidi2_ci_direct_uint16_at(buf + 16 + headerSize, numChunks);
    cmidi2_ci_direct_uint16_at(buf + 18 + headerSize, chunkIndex);
    cmidi2_ci_direct_uint16_at(buf + 20 + headerSize, dataSize);
    memcpy(buf + 22 + headerSize, data, dataSize);
}

static inline int32_t cmidi2_ci_try_parse_new_protocol(uint8_t* buf, int32_t length) {
    return (length != 19 || buf[0] != 0x7E || buf[1] != 0x7F || buf[2] != CMIDI2_CI_SUB_ID ||
            buf[3] != CMIDI2_CI_SUB_ID_2_SET_NEW_PROTOCOL || buf[4] != 1) ? 0 : buf[14];
}

