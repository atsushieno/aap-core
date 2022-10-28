#include <sys/mman.h>
#include "aap/unstable/logging.h"
#include "aap/ext/aap-midi2.h"
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

    void AAPMidiProcessor::initialize(aap::PluginClientConnectionList* connections, int32_t sampleRate, int32_t audioOutChannelCount, int32_t aapFrameSize, int32_t midiBufferSize) {
        assert(connections);
        plugin_list = aap::PluginListSnapshot::queryServices();

        // AAP settings
        client = std::make_unique<aap::PluginClient>(connections, &plugin_list);
        sample_rate = sampleRate;
        aap_frame_size = aapFrameSize;
        midi_buffer_size = midiBufferSize;
        channel_count = audioOutChannelCount;

        aap_input_ring_buffer = zix_ring_new(aap_frame_size * audioOutChannelCount * sizeof(float) * 2); // xx for ring buffering
        zix_ring_mlock(aap_input_ring_buffer);
        interleave_buffer = (float*) calloc(sizeof(float), aapFrameSize * audioOutChannelCount);

        translation_buffer = (uint8_t*) calloc(1, midiBufferSize);

        // Oboe configuration
        pal()->setupStream();
    }

    void AAPMidiProcessor::terminate() {
        if (instance_data != nullptr) {
            if (instance_data->instance_id >= 0) {
                auto instance = client->getInstance(instance_data->instance_id);
                if (!instance)
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "instance of instance_id %d was not found",
                                 instance_data->instance_id);
                else
                    instance->dispose();
            }
            else
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "detected unexpected instance_id: %d",
                             instance_data->instance_id);
        }
        if (aap_input_ring_buffer)
            zix_ring_free(aap_input_ring_buffer);
        if (interleave_buffer)
            free(interleave_buffer);
        if (translation_buffer)
            free(translation_buffer);

        client.reset();

        aap::a_log_f(AAP_LOG_LEVEL_INFO, "AAPMidiProcessor", "Successfully terminated MIDI processor.");
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
        aap::a_log_f(AAP_LOG_LEVEL_INFO, "AAPMidiProcessor", "instantiating plugin %s", pluginId.c_str());

        if (state != AAP_MIDI_PROCESSOR_STATE_CREATED) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "Unexpected call to instantiatePlugin() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        if (instance_data) {
            const auto& instance = client->getInstance(instance_data->instance_id);
            if (instance)
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "There is an already instantiated plugin \"%s\" for this MidiDeviceService.",
                             instance->getPluginInformation()->getDisplayName().c_str());
            else
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "Internal error: stale plugin instance data remains in the memory.",
                             instance->getPluginInformation()->getDisplayName().c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        auto pluginInfo = plugin_list.getPluginInformation(pluginId);
        if (!pluginInfo) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "Plugin of ID \"%s\" is not found.", pluginId.c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }
        if (!pluginInfo->isInstrument()) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "Plugin \"%s\" is not an instrument.",
                         pluginInfo->getDisplayName().c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        aap::a_log_f(AAP_LOG_LEVEL_INFO, "AAPMidiProcessor", "host is going to instantiate %s", pluginId.c_str());
        client->createInstanceAsync(pluginId, sample_rate, true, [&](int32_t instanceId, std::string error) {
            if (instanceId < 0) {
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor",
                             "Plugin \"%s\" could not be instantiated.",
                             pluginInfo->getDisplayName().c_str());
                state = AAP_MIDI_PROCESSOR_STATE_ERROR;
                return;
            }
            auto instance = dynamic_cast<aap::RemotePluginInstance*>(client->getInstance(instanceId));
            assert(instance);

            instrument_instance_id = instanceId;

            instance->completeInstantiation();

            instance->configurePorts();

            int32_t numPorts = instance->getNumPorts();
            auto data = std::make_unique<PluginInstanceData>(instanceId, numPorts);

            data->instance_id = instanceId;
            data->plugin_buffer = instance->getAudioPluginBuffer(numPorts, aap_frame_size, midi_buffer_size);

            for (int i = 0; i < numPorts; i++) {
                auto port = instance->getPort(i);
                if (port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO &&
                    port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT)
                    data->getAudioOutPorts()->emplace_back(i);
                else if (port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI2 &&
                         port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
                    data->midi2_in_port = i;
                else if (port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI &&
                         port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
                    data->midi1_in_port = i;
            }

            // FIXME: fill default parameter values (now that we don't have them in ports).

            instance->prepare(aap_frame_size, data->plugin_buffer);

            instance_data = std::move(data);

            state = AAP_MIDI_PROCESSOR_STATE_INACTIVE;

            aap::a_log_f(AAP_LOG_LEVEL_INFO, "AAPMidiProcessor", "instantiated plugin %s",
                         pluginId.c_str());
        });
    }

    // Activate audio processing. Starts audio (oboe) streaming, CPU-intensive operations happen from here.
    void AAPMidiProcessor::activate() {
        if (state != AAP_MIDI_PROCESSOR_STATE_INACTIVE) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "Unexpected call to activate() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        auto startStreamingResult = pal()->startStreaming();
        if (startStreamingResult) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "startStreaming() failed with error code %d.",
                         startStreamingResult);
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        // activate instances
        for (int i = 0; i < client->getInstanceCount(); i++)
            client->getInstance(i)->activate();

        state = AAP_MIDI_PROCESSOR_STATE_ACTIVE;
    }

    // Deactivate audio processing. CPU-intensive operations stop here.
    void AAPMidiProcessor::deactivate() {
        if (state != AAP_MIDI_PROCESSOR_STATE_ACTIVE) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "Unexpected call to deactivate() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        // deactivate AAP instances
        for (int i = 0; i < client->getInstanceCount(); i++)
            client->getInstance(i)->deactivate();

        pal()->stopStreaming();

        state = AAP_MIDI_PROCESSOR_STATE_INACTIVE;
    }

    int32_t failed_plugin_process_count;
    // Called by Oboe audio callback implementation. It calls process.
    void AAPMidiProcessor::callPluginProcess() {
        auto data = instance_data.get();
        if (!data) {
            // It's not ready to process audio yet.
            if (failed_plugin_process_count++ < 10)
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "callPluginProcess() failed. Plugin instance data Not ready uet.");
            return;
        }

        client->getInstance(data->instance_id)->process(data->plugin_buffer, 1000000000);
        // reset MIDI buffers after plugin process (otherwise it will send the same events in the next iteration).
        if (data->instance_id == instrument_instance_id) {
            if (data->midi1_in_port >= 0)
                ((AAPMidiBufferHeader*) data->plugin_buffer->buffers[data->midi1_in_port])->length = 0;
            if (data->midi2_in_port >= 0)
                ((AAPMidiBufferHeader*) data->plugin_buffer->buffers[data->midi2_in_port])->length = 0;
        } else {
            if (failed_plugin_process_count++ < 10)
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "callPluginProcess() is invoked while there is no instrument plugin instantiated.");
            return;
        }
    }

    int32_t failed_audio_output_count{0};

    // Called by Oboe audio callback (`onAudioReady()`) implementation. It is called after AAP processing, and
    //  fill the audio outputs into an intermediate buffer, interleaving the results,
    //  then copied into the ring buffer.
    void AAPMidiProcessor::fillAudioOutput() {
        // FIXME: the final processing result should not be the instrument instance output buffer
        //  but should be chained output result. Right now we don't support chaining.

        memset(interleave_buffer, 0, channel_count * aap_frame_size * sizeof(float));

        auto data = instance_data.get();
        if (!data) {
            // It's not ready to process audio yet.
            if (failed_audio_output_count++ < 10)
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "fillAudioOutput() for Oboe audio callback failed. Plugin instance data Not ready uet.");
            return;
        }
        else
            failed_audio_output_count = 0;

        if (data->instance_id == instrument_instance_id) {
            int numPorts = data->getAudioOutPorts()->size();
            for (int p = 0; p < numPorts; p++) {
                int portIndex = data->getAudioOutPorts()->at(p);
                auto src = (float*) data->plugin_buffer->buffers[portIndex];
                // We have to interleave separate port outputs to copy...
                for (int i = 0; i < aap_frame_size; i++)
                    interleave_buffer[i * numPorts + p] = src[i];
            }
            failed_audio_output_count = 0;
        } else {
            if (failed_audio_output_count++ < 10)
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "fillAudioOutput() is invoked while there is no instrument plugin instantiated.");
            return;
        }

        zix_ring_write(aap_input_ring_buffer, interleave_buffer, channel_count * aap_frame_size * sizeof(float));
    }

    int32_t AAPMidiProcessor::getAAPMidiInputPortType() {
        auto data = instance_data.get();
        assert(data);
        // Returns
        // - MIDI2 port if MIDI2 port exists and MIDI2 protocol is specified.
        // - MIDI1 port unless MIDI1 port does not exist.
        // - MIDI2 port (translation needed).
        return (receiver_midi_protocol == CMIDI2_PROTOCOL_TYPE_MIDI2 && data->midi2_in_port >= 0) ?
            CMIDI2_PROTOCOL_TYPE_MIDI2 :
            data->midi1_in_port >= 0 ? CMIDI2_PROTOCOL_TYPE_MIDI1 :
            CMIDI2_PROTOCOL_TYPE_MIDI2;
    }

    void* AAPMidiProcessor::getAAPMidiInputBuffer() {
        auto data = instance_data.get();
        assert(data);
        int portIndex = getAAPMidiInputPortType() == CMIDI2_PROTOCOL_TYPE_MIDI2 ? data->midi2_in_port : data->midi1_in_port;
        return data->plugin_buffer->buffers[portIndex];
    }

    size_t AAPMidiProcessor::translateMidiBufferIfNeeded(uint8_t* bytes, size_t offset, size_t length) {
        if (receiver_midi_protocol == CMIDI2_PROTOCOL_TYPE_MIDI2) {
            // It receives UMPs. If port is MIDI1, we translate to MIDI1 bytestream.
            if (getAAPMidiInputPortType() != CMIDI2_PROTOCOL_TYPE_MIDI2) {
                cmidi2_midi_conversion_context context;
                cmidi2_midi_conversion_context_initialize(&context);
                context.ump = (cmidi2_ump*) bytes + offset;
                context.ump_num_bytes = length;
                context.midi1 = translation_buffer;
                context.midi1_num_bytes = sizeof(translation_buffer);
                context.group = 0;

                if (cmidi2_convert_ump_to_midi1(&context) != CMIDI2_CONVERSION_RESULT_OK) {
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "Failed to translate MIDI 2.0 UMP inputs to MIDI 1.0 stream");
                    return 0;
                }
                memcpy(bytes + offset, translation_buffer, context.ump_proceeded_bytes);
                return context.midi1_proceeded_bytes;
            }
        } else {
            // It receives MIDI1 bytestream. If port is MIDI2, we translate to UMPs.
            if (getAAPMidiInputPortType() == CMIDI2_PROTOCOL_TYPE_MIDI2) {
                cmidi2_midi_conversion_context context;
                cmidi2_midi_conversion_context_initialize(&context);
                context.midi1 = bytes + offset;
                context.midi1_num_bytes = length;
                context.ump = (cmidi2_ump*) translation_buffer;
                context.ump_num_bytes = sizeof(translation_buffer);
                context.group = 0;

                if (cmidi2_convert_midi1_to_ump(&context) != CMIDI2_CONVERSION_RESULT_OK) {
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAPMidiProcessor", "Failed to translate MIDI 1.0 inputs to MIDI 2.0 UMPs");
                    return 0;
                }
                memcpy(bytes + offset, translation_buffer, context.ump_proceeded_bytes);
                return context.ump_proceeded_bytes;
            }
        }
        return 0;
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
        pal()->midiInputReceived(bytes, offset, length, timestampInNanoseconds);

        size_t translated = translateMidiBufferIfNeeded(bytes, offset, length);
        if (translated > 0)
            length = translated;

        // it is 99.999... percent true since audio loop must have started before any MIDI events...
        if (last_aap_process_time.tv_sec > 0) {
            int64_t diff = (curtime.tv_sec - last_aap_process_time.tv_sec) * 1000000000 +
                        curtime.tv_nsec - last_aap_process_time.tv_nsec;
            auto nanosecondsPerCycle = (int64_t) (1.0 * aap_frame_size / sample_rate * 1000000000);
            actualTimestamp = (timestampInNanoseconds + diff) % nanosecondsPerCycle;
        }

        // FIXME: rewrite this part to work fine along with realtime consumer.
        //  Currently read (consomption) occurs while it is preparing for MIDI buffer and things become inconsistent.
        auto dst8 = (uint8_t *) getAAPMidiInputBuffer();
        auto dstMBH = (AAPMidiBufferHeader*) dst8;
        if (dst8 != nullptr) {
            uint32_t currentOffset = dstMBH->length;
            if (getAAPMidiInputPortType() == CMIDI2_PROTOCOL_TYPE_MIDI2) {
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
                    receiver_midi_protocol = maybeNewProtocol;
                // Note that Set New Protocol has to be also sent to the recipient too.

                dstMBH->time_options = -100;
                uint32_t ticks = timestampInNanoseconds / (44100 / -dstMBH->time_options);
                size_t lengthSize = cmidi2_midi1_write_7bit_encoded_int(dst8 + 8 + currentOffset, ticks);
                memcpy(dst8 + 32 + currentOffset + lengthSize, bytes + offset, length);
                dstMBH->length += length + lengthSize;
            }
        }
    }
}
