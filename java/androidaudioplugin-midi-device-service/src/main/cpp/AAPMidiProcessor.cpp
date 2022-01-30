#include <sys/mman.h>
#include <aap/logging.h>
#include <aap/aap-midi2.h>
#include "AAPMidiProcessor.h"

namespace aapmidideviceservice {
    long last_delay_value = 0, worst_delay_value = 0;
    long success_count = 0, failure_count = 0;

    int32_t AAPMidiProcessor::onAudioReady(void *audioData, int32_t numFrames) {
        if (state != AAP_MIDI_PROCESSOR_STATE_ACTIVE)
            // it is not supposed to process audio at this state.
            // It is still possible that it gets called between Oboe requestStart()
            // and updating the state, so do not error out here.
            return AudioCallbackResult::Continue;

        // Oboe audio buffer request can be extremely small (like < 10 frames).
        //  Therefore we don't call plugin process() every time (as it could cost hundreds
        //  of microseconds). Instead, we make use of ring buffer, call plugin process()
        //  only once in a while (when our ring buffer starves).
        //
        //  Each plugin process is still expected to fit within a callback time slice,
        //  so we still call plugin process() within the callback.

        if (zix_ring_read_space(aap_input_ring_buffer) < numFrames * sizeof(float)) {
            // observer performance. (start)
            struct timespec ts_start{}, ts_end{};
            clock_gettime(CLOCK_REALTIME, &ts_start);

            callPluginProcess();
            // recorded for later reference at MIDI message buffering.
            clock_gettime(CLOCK_REALTIME, &last_aap_process_time);

            // observer performance. (end)
            clock_gettime(CLOCK_REALTIME, &ts_end);
            long diff = (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 + ts_end.tv_nsec - ts_start.tv_nsec;
            if (diff > 1000000) { // you took 1msec!?
                last_delay_value = diff;
                if (diff > worst_delay_value)
                    worst_delay_value = diff;
                failure_count++;
            } else success_count++;

            fillAudioOutput();
        }

        zix_ring_read(aap_input_ring_buffer, audioData, numFrames * sizeof(float));

        // FIXME: can we terminate it when it goes quiet?
        return AudioCallbackResult::Continue;
    }

    void AAPMidiProcessor::initialize(int32_t sampleRate, int32_t audioOutChannelCount, int32_t aapFrameSize) {
        // AAP settings
        host = std::make_unique<aap::PluginHost>(&host_manager);
        sample_rate = sampleRate;
        aap_frame_size = aapFrameSize;
        channel_count = audioOutChannelCount;

        aap_input_ring_buffer = zix_ring_new(aap_frame_size * audioOutChannelCount * sizeof(float) * 2); // xx for ring buffering
        zix_ring_mlock(aap_input_ring_buffer);
        interleave_buffer = (float*) calloc(sizeof(float), aapFrameSize * audioOutChannelCount);

        // Oboe configuration
        pal()->setupStream();
    }

    void AAPMidiProcessor::terminate() {

        for (auto& data : instance_data_list)
            host->getInstance(data->instance_id)->dispose();

        zix_ring_free(aap_input_ring_buffer);
        free(interleave_buffer);

        host.reset();
    }

    std::string AAPMidiProcessor::convertStateToText(AAPMidiProcessorState stateValue) {
        switch (stateValue) {
            case AAP_MIDI_PROCESSOR_STATE_CREATED:
                return "CREATED";
            case AAP_MIDI_PROCESSOR_STATE_ACTIVE:
                return "ACTIVE";
            case AAP_MIDI_PROCESSOR_STATE_INACTIVE:
                return "INACTIVE";
            case AAP_MIDI_PROCESSOR_STATE_STOPPED:
                return "STOPPED";
            case AAP_MIDI_PROCESSOR_STATE_ERROR:
                return "ERROR";
        }
        return "(UNKNOWN)";
    }

    // Instantiate AAP plugin and proceed up to prepare().
    void AAPMidiProcessor::instantiatePlugin(std::string pluginId) {

        if (state != AAP_MIDI_PROCESSOR_STATE_CREATED) {
            aap::aprintf("Unexpected call to start() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        auto pluginInfo = host_manager.getPluginInformation(pluginId);
        int32_t numPorts = pluginInfo->getNumPorts();
        if (pluginInfo->isInstrument())
            if (instrument_instance_id != 0) {
                const auto& info = host->getInstance(instrument_instance_id)->getPluginInformation();
                aap::aprintf("Instrument instance %s is already assigned.",
                             info->getDisplayName().c_str());
                state = AAP_MIDI_PROCESSOR_STATE_ERROR;
                return;
            }

        auto instanceId = host->createInstance(pluginId, sample_rate, true);
        auto instance = host->getInstance(instanceId);

        if (pluginInfo->isInstrument())
            instrument_instance_id = instanceId;

        auto data = std::make_unique<PluginInstanceData>(instanceId, numPorts);

        instance->completeInstantiation();

        data->instance_id = instanceId;
        data->plugin_buffer = instance->getSharedMemoryBuffer(numPorts, aap_frame_size);


        for (int i = 0; i < numPorts; i++) {
            auto port = pluginInfo->getPort(i);
            if (port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO && port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT)
                data->getAudioOutPorts()->emplace_back(i);
            else if (port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI2 && port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
                data->midi2_in_port = i;
            else if (port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI && port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
                data->midi1_in_port = i;
            else if (port->hasProperty(AAP_PORT_DEFAULT))
                *((float*) data->plugin_buffer->buffers[i]) = port->getDefaultValue();
        }

        instance->prepare(aap_frame_size, data->plugin_buffer);

        instance_data_list.emplace_back(std::move(data));
    }

    // Activate audio processing. CPU-intensive operations happen from here.
    void AAPMidiProcessor::activate() {
        // start Oboe stream.
        switch (state) {
            case AAP_MIDI_PROCESSOR_STATE_CREATED:
            case AAP_MIDI_PROCESSOR_STATE_INACTIVE:
                break;
            default:
                aap::aprintf("Unexpected call to start() at %s state.",
                             convertStateToText(state).c_str());
                state = AAP_MIDI_PROCESSOR_STATE_ERROR;
                return;
        }

        if (pal()->startStreaming()) {
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        // activate instances
        for (int i = 0; i < host->getInstanceCount(); i++)
            host->getInstance(i)->activate();

        state = AAP_MIDI_PROCESSOR_STATE_ACTIVE;
    }

    // Deactivate audio processing. CPU-intensive operations stop here.
    void AAPMidiProcessor::deactivate() {
        if (state != AAP_MIDI_PROCESSOR_STATE_ACTIVE) {
            aap::aprintf("Unexpected call to deactivate() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        // deactivate AAP instances
        for (int i = 0; i < host->getInstanceCount(); i++)
            host->getInstance(i)->deactivate();

        pal()->stopStreaming();

        state = AAP_MIDI_PROCESSOR_STATE_INACTIVE;
    }

    // Called by Oboe audio callback implementation. It calls process.
    void AAPMidiProcessor::callPluginProcess() {
        for (auto &data : instance_data_list) {
            host->getInstance(data->instance_id)->process(data->plugin_buffer, 1000000000);
            // reset MIDI buffers after plugin process (otherwise it will send the same events in the next iteration).
            if (data->instance_id == instrument_instance_id) {
                if (data->midi1_in_port >= 0)
                    memset((void *) data->plugin_buffer->buffers[data->midi1_in_port], 0,
                           aap_frame_size * sizeof(float));
                if (data->midi2_in_port >= 0)
                    memset((void *) data->plugin_buffer->buffers[data->midi2_in_port], 0,
                           aap_frame_size * sizeof(float));
            }
        }
    }

    // Called by Oboe audio callback implementation. It is called after AAP processing, and
    //  fill the audio outputs into an intermediate buffer, interleaving the results,
    //  then copied into the ring buffer.
    void AAPMidiProcessor::fillAudioOutput() {
        // FIXME: the final processing result should not be the instrument instance output buffer
        //  but should be chained output result. Right now we don't support chaining.

        memset(interleave_buffer, 0, channel_count * aap_frame_size * sizeof(float));

        for (auto &data : instance_data_list) {
            if (data->instance_id == instrument_instance_id) {
                int numPorts = data->getAudioOutPorts()->size();
                for (int p = 0; p < numPorts; p++) {
                    int portIndex = data->getAudioOutPorts()->at(p);
                    auto src = (float*) data->plugin_buffer->buffers[portIndex];
                    // We have to interleave separate port outputs to copy...
                    for (int i = 0; i < aap_frame_size; i++)
                        interleave_buffer[i * numPorts + p] = src[i];
                }
            }
        }

        zix_ring_write(aap_input_ring_buffer, interleave_buffer, channel_count * aap_frame_size * sizeof(float));
    }

    int getTicksFromNanoseconds(int deltaTimeSpec, uint64_t value) {
        double bpm = 120.0;
        auto ticksPerSec = deltaTimeSpec / (bpm / 60);
        auto ret = ticksPerSec * value / 1000000000;
        return (int) ret;
    }

    int set7BitEncodedLength(uint8_t* buffer, int value) {
        int pos = 0;
        do {
            buffer[pos] = value % 80;
            value /= 0x80;
            if (value > 0)
                buffer[pos] &= 0x80;
            pos++;
        } while (value != 0);
        return pos;
    }

    PluginInstanceData* AAPMidiProcessor::getAAPMidiInputData() {
        for (auto &data : instance_data_list)
            if (data->instance_id == instrument_instance_id)
                return data.get();
        return nullptr;
    }

    void* AAPMidiProcessor::getAAPMidiInputBuffer() {
        for (auto &data : instance_data_list)
            if (data->instance_id == instrument_instance_id) {
                // MIDI2 port if If MIDI2 port exists and MIDI2 protocol is specified.
                // MIDI1 port unless MIDI1 port does not exist.
                // MIDI2 port (or -1 if none exists).
                int portIndex = (midi_protocol == CMIDI2_PROTOCOL_TYPE_MIDI2 && data->midi2_in_port >= 0) ? data->midi2_in_port : data->midi1_in_port >= 0 ? data->midi1_in_port : data->midi2_in_port;
                return data->plugin_buffer->buffers[portIndex];
            }
        return nullptr;
    }

    void AAPMidiProcessor::processMidiInput(uint8_t* bytes, size_t offset, size_t length, int64_t timestampInNanoseconds) {
        // This function is invoked every time Android MidiReceiver.onSend() is invoked, immediately.
        // On the other hand, AAPs don't process MIDI messages immediately, so we have to buffer
        // them and flush them to the instrument plugin every time process() is invoked.
        //
        // Since we don't really know when it is actually processed but have to accurately
        // pass delta time for each input events, we assign event timecode from
        // 1) argument timestampInNanoseconds and 2) actual time passed from previous call to
        // this function.
        int64_t actualTimestamp = timestampInNanoseconds;
        struct timespec curtime{};
        clock_gettime(CLOCK_REALTIME, &curtime);
        // it is 99.999... percent true since audio loop must have started before any MIDI events...
        if (last_aap_process_time.tv_sec > 0) {
            int64_t diff = (curtime.tv_sec - last_aap_process_time.tv_sec) * 1000000000 +
                        curtime.tv_nsec - last_aap_process_time.tv_nsec;
            auto nanosecondsPerCycle = (int64_t) (1.0 * aap_frame_size / sample_rate * 1000000000);
            actualTimestamp = (timestampInNanoseconds + diff) % nanosecondsPerCycle;
        }

        auto dst8 = (uint8_t *) getAAPMidiInputBuffer();
        auto dstMBH = (AAPMidiBufferHeader*) dst8;
        if (dst8 != nullptr) {
            uint32_t currentOffset = dstMBH->length;
            if (midi_protocol == CMIDI2_PROTOCOL_TYPE_MIDI2) {
                int32_t tIter = 0;
                for (int64_t ticks = actualTimestamp / (1000000000 / 31250);
                     ticks > 0; ticks -= 31250, tIter++) {
                    *(int32_t *) (dst8 + 32 + currentOffset + tIter * 4) =
                            (int32_t) cmidi2_ump_jr_timestamp_direct(0,
                                                                     ticks > 31250 ? 31250 : ticks);
                }
                dstMBH->length += tIter * 4;
                currentOffset += tIter * 4;
                // process MIDI 2.0 data
                memcpy(dst8 + 32 + currentOffset, bytes + offset, length);
                dstMBH->length += length;
                dstMBH->time_options = 0; // reserved in MIDI2 mode
                for (int i = 2; i < 8; i++)
                    dstMBH->reserved[i - 2] = 0; // reserved
            } else {
                // See if it is MIDI-CI Set New Protocol (must be a standalone message).
                // If so, update protocol and return immediately.
                int32_t maybeNewProtocol;
                if (bytes[offset] == 0xF0 && bytes[length - 1] == 0xF7 &&
                        (maybeNewProtocol = cmidi2_ci_try_parse_new_protocol(bytes + offset + 1, length - 2)) > 0)
                    midi_protocol = maybeNewProtocol;
                // Note that Set New Protocol has to be also sent to the recipient too.

                dstMBH->time_options = -100;
                uint32_t ticks = timestampInNanoseconds / (44100 / -dstMBH->time_options);
                size_t lengthSize = cmidi2_midi1_write_7bit_encoded_int(dst8 + 8 + currentOffset, ticks);
                memcpy(dst8 + 8 + currentOffset + lengthSize, bytes + offset, length);
                dstMBH->length += length + lengthSize;
            }
        }
    }
}
