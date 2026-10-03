#ifndef AAP_CORE_INSTANCE_REALTIME_STATE_H
#define AAP_CORE_INSTANCE_REALTIME_STATE_H

#include "realtime-byte-queue.h"
#include "instance-extension-worker.h"
#include "processing-quiescence.h"
#include "aap/core/aap_midi2_helper.h"
#include "aap/ext/midi.h"

namespace aap::internal {
struct HostNotification {
    char uri[AAP_MIDI2_AAPXS_DATA_MAX_SIZE];
    int32_t opcode;
    uint32_t request_id;
};
struct InstanceRealtimeState {
    explicit InstanceRealtimeState(size_t midiCapacity)
        : ump_input(midiCapacity), ump_output(midiCapacity), gui_output(midiCapacity) {}
    // Whole validated AAPXS messages only, not borrowed audio/MIDI port buffers.
    RealtimeByteQueue<64> aapxs_input{2 * AAP_MIDI2_AAPXS_DATA_MAX_SIZE + sizeof(AAPMidiBufferHeader)};
    RealtimeByteQueue<64> ump_input, ump_output, gui_output;
    RealtimeByteQueue<64> host_notifications{sizeof(HostNotification)};
    std::atomic<uint32_t> standard_notifications{0};
    std::atomic<bool> process_notification{false};
    std::atomic<bool> layout_refresh{false};
    InstanceExtensionWorker worker;
    ProcessingQuiescence processing;
    std::mutex gui_read_mutex; // GUI consumers only; processing only publishes
    size_t gui_read_offset{0};
    std::mutex parameter_scan_mutex; // control/worker scans of this instance only
};

inline void queueAAPXSMidi2Input(void* context, const void* data, size_t size) {
    auto& state = *static_cast<InstanceRealtimeState*>(context);
    AAPMidiBufferHeader header{};
    header.length = static_cast<uint32_t>(size);
    if (state.aapxs_input.tryPush(data, size, &header, sizeof(header))) state.worker.notify();
}
}
#endif
