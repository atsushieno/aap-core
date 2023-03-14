#include <sys/mman.h>
#include <mutex>
#include "aap/unstable/logging.h"
#include "aap/ext/midi.h"
#include "AAPMidiProcessor.h"

#define LOG_TAG "AAPMidiProcessor"

namespace aap::midi {
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

        // FIXME: I don't think we need this ring buffer anymore but removing this still resulted in audio glitches.
        if (zix_ring_read_space(aap_input_ring_buffer) < numFrames * sizeof(float)) {

#if ANDROID
            struct timespec timeSpecBegin{}, timeSpecEnd{};
            if (ATrace_isEnabled()) {
                ATrace_beginSection("aap::midi::AAPMidiProcessor_callPluginProcess");
                clock_gettime(CLOCK_REALTIME, &timeSpecBegin);
            }
#endif
            callPluginProcess();

            // recorded for later reference at MIDI message buffering.
            clock_gettime(CLOCK_REALTIME, &last_aap_process_time);

            // observer performance. (end)
#if ANDROID
            if (ATrace_isEnabled()) {
                clock_gettime(CLOCK_REALTIME, &timeSpecEnd);
                ATrace_setCounter("aap::midi::AAPMidiProcessor_callPluginProcess",
                                  (timeSpecEnd.tv_sec - timeSpecBegin.tv_sec) * 1000000000 +
                                  timeSpecEnd.tv_nsec - timeSpecBegin.tv_nsec);
                ATrace_endSection();

                long diff = (timeSpecEnd.tv_sec - timeSpecBegin.tv_sec) * 1000000000 +
                            timeSpecEnd.tv_nsec - timeSpecBegin.tv_nsec;
                if (diff > 1000000) { // you took 1msec!?
                    last_delay_value = diff;
                    if (diff > worst_delay_value)
                        worst_delay_value = diff;
                    failure_count++;
                } else success_count++;
            }
#endif

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

        memset(midi_input_buffer, 0, sizeof(midi_input_buffer));

        // Oboe configuration
        pal()->setupStream();
    }

    void AAPMidiProcessor::terminate() {
        if (instance_data != nullptr) {
            if (instance_data->instance_id >= 0) {
                auto instance = client->getInstanceById(instance_data->instance_id);
                if (!instance)
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "instance of instance_id %d was not found",
                                 instance_data->instance_id);
                else
                    instance->dispose();
            }
            else
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "detected unexpected instance_id: %d",
                             instance_data->instance_id);
        }
        if (aap_input_ring_buffer)
            zix_ring_free(aap_input_ring_buffer);
        if (interleave_buffer)
            free(interleave_buffer);
        if (translation_buffer)
            free(translation_buffer);

        client.reset();

        aap::a_log_f(AAP_LOG_LEVEL_INFO, LOG_TAG, "Successfully terminated MIDI processor.");
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
        aap::a_log_f(AAP_LOG_LEVEL_INFO, LOG_TAG, "instantiating plugin %s", pluginId.c_str());

        if (state != AAP_MIDI_PROCESSOR_STATE_CREATED) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "Unexpected call to instantiatePlugin() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        if (instance_data) {
            const auto& instance = client->getInstanceById(instance_data->instance_id);
            if (instance)
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "There is an already instantiated plugin \"%s\" for this MidiDeviceService.",
                             instance->getPluginInformation()->getDisplayName().c_str());
            else
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "Internal error: stale plugin instance data remains in the memory.",
                             instance->getPluginInformation()->getDisplayName().c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        auto pluginInfo = plugin_list.getPluginInformation(pluginId);
        if (!pluginInfo) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "Plugin of ID \"%s\" is not found.", pluginId.c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }
        if (!pluginInfo->isInstrument()) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "Plugin \"%s\" is not an instrument.",
                         pluginInfo->getDisplayName().c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        aap::a_log_f(AAP_LOG_LEVEL_INFO, LOG_TAG, "host is going to instantiate %s", pluginId.c_str());
        std::function<void(std::string&)> cb = [&](std::string& error) {
            if (!error.empty()) {
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG,R"(Plugin service for "%s" ("%s") could not be connected.)",
                             pluginInfo->getDisplayName().c_str(), pluginInfo->getPluginPackageName().c_str());
                state = AAP_MIDI_PROCESSOR_STATE_ERROR;
                return;
            }
            auto result = client->createInstance(pluginId, sample_rate, true);
            if (!result.error.empty()) {
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG,"Plugin \"%s\" could not be instantiated: %s",
                             pluginInfo->getDisplayName().c_str(), result.error.c_str());
                state = AAP_MIDI_PROCESSOR_STATE_ERROR;
                return;
            }
            auto instanceId = result.value;
            auto instance = dynamic_cast<aap::RemotePluginInstance*>(client->getInstanceById(instanceId));
            assert(instance);

            instrument_instance_id = instanceId;

            instance->configurePorts();

            int32_t numPorts = instance->getNumPorts();
            auto data = std::make_unique<PluginInstanceData>(instanceId, numPorts);

            data->instance_id = instanceId;

            for (int i = 0; i < numPorts; i++) {
                auto port = instance->getPort(i);
                if (port->getContentType() == AAP_CONTENT_TYPE_AUDIO &&
                    port->getPortDirection() == AAP_PORT_DIRECTION_OUTPUT)
                    data->getAudioOutPorts()->emplace_back(i);
                else if (port->getContentType() == AAP_CONTENT_TYPE_MIDI2 &&
                         port->getPortDirection() == AAP_PORT_DIRECTION_INPUT)
                    data->midi2_in_port = i;
                else if (port->getContentType() == AAP_CONTENT_TYPE_MIDI &&
                         port->getPortDirection() == AAP_PORT_DIRECTION_INPUT)
                    data->midi1_in_port = i;
            }

            instance->prepare(aap_frame_size);

            instance_data = std::move(data);

            state = AAP_MIDI_PROCESSOR_STATE_INACTIVE;

            aap::a_log_f(AAP_LOG_LEVEL_INFO, LOG_TAG, "instantiated plugin %s", pluginId.c_str());
        };
        client->connectToPluginService(pluginId, cb);
    }

    // Note that it is an expensive operation so we cache it at activate().
    int32_t AAPMidiProcessor::getInstrumentMidiMappingPolicy() {
        for (int i = 0; i < client->getInstanceCount(); i++) {
            auto instance = client->getInstanceByIndex(i);
            if (instance->getPluginInformation()->isInstrument())
                return instance->getStandardExtensions().getMidiMappingPolicy(instance->getPluginInformation()->getPluginID());
        }
        return 0;
    }

    // Activate audio processing. Starts audio (oboe) streaming, CPU-intensive operations happen from here.
    void AAPMidiProcessor::activate() {
        if (state != AAP_MIDI_PROCESSOR_STATE_INACTIVE) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "Unexpected call to activate() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        current_mapping_policy = getInstrumentMidiMappingPolicy();

        auto startStreamingResult = pal()->startStreaming();
        if (startStreamingResult) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "startStreaming() failed with error code %d.",
                         startStreamingResult);
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        // activate instances
        for (int i = 0; i < client->getInstanceCount(); i++)
            client->getInstanceByIndex(i)->activate();

        state = AAP_MIDI_PROCESSOR_STATE_ACTIVE;
    }

    // Deactivate audio processing. CPU-intensive operations stop here.
    void AAPMidiProcessor::deactivate() {
        if (state != AAP_MIDI_PROCESSOR_STATE_ACTIVE) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "Unexpected call to deactivate() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        // deactivate AAP instances
        for (int i = 0; i < client->getInstanceCount(); i++)
            client->getInstanceByIndex(i)->deactivate();

        pal()->stopStreaming();

        state = AAP_MIDI_PROCESSOR_STATE_INACTIVE;
    }

    int32_t failed_plugin_process_count;
    // Called by Oboe audio callback implementation. It calls process.
    void AAPMidiProcessor::callPluginProcess() {

        auto dstBuffer = (AAPMidiBufferHeader*) getAAPMidiInputBuffer();
        if (std::unique_lock<NanoSleepLock> tryLock(midi_buffer_mutex, std::try_to_lock); tryLock.owns_lock()) {
            auto srcBuffer = (AAPMidiBufferHeader*) midi_input_buffer;
            if (srcBuffer->length)
                memcpy(dstBuffer + 1, srcBuffer + 1, srcBuffer->length);
            dstBuffer->length = srcBuffer->length;
            srcBuffer->length = 0;
        } else {
            // failed to acquire lock; we do not send anything this time.
            dstBuffer->length = 0;
        }
        dstBuffer->time_options = 0; // reserved in MIDI2 mode

        auto data = instance_data.get();
        if (!data) {
            // It's not ready to process audio yet.
            if (failed_plugin_process_count++ < 10)
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "callPluginProcess() failed. Plugin instance data Not ready uet.");
            return;
        }

        auto instance = client->getInstanceById(data->instance_id);
        instance->process(aap_frame_size, 1000000000);
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
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "fillAudioOutput() for Oboe audio callback failed. Plugin instance data Not ready uet.");
            return;
        }
        else
            failed_audio_output_count = 0;

        if (data->instance_id == instrument_instance_id) {
            int numPorts = data->getAudioOutPorts()->size();
            auto instance = client->getInstanceById(data->instance_id);
            auto b = instance->getAudioPluginBuffer();
            for (int p = 0; p < numPorts; p++) {
                int portIndex = data->getAudioOutPorts()->at(p);
                auto src = (float*) b->get_buffer(*b, portIndex);
                // We have to interleave separate port outputs to copy...
                for (int i = 0; i < aap_frame_size; i++)
                    interleave_buffer[i * numPorts + p] = src[i];
            }
            failed_audio_output_count = 0;
        } else {
            if (failed_audio_output_count++ < 10)
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "fillAudioOutput() is invoked while there is no instrument plugin instantiated.");
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
        auto instance = client->getInstanceById(data->instance_id);
        auto b = instance->getAudioPluginBuffer();
        return b->get_buffer(*b, portIndex);
    }

    size_t AAPMidiProcessor::runThroughMidi2UmpForMidiMapping(uint8_t* bytes, size_t offset, size_t length) {
        int32_t translatedIndex = 0;
        CMIDI2_UMP_SEQUENCE_FOREACH(bytes + offset, length, iter) {
            auto ump = (cmidi2_ump*) iter;
            int32_t parameterIndex = -1;
            uint32_t parameterValueI32 = 0;
            int32_t parameterKey = 0;
            int32_t parameterExtra = 0;
            int32_t presetIndex = -1;
            switch (cmidi2_ump_get_message_type(ump)) {
                case CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL:
                    switch (cmidi2_ump_get_status_code(ump)) {
                        case CMIDI2_STATUS_CC:
                            if ((current_mapping_policy & AAP_PARAMETERS_MAPPING_POLICY_CC) != 0) {
                                parameterIndex = cmidi2_ump_get_midi2_cc_index(ump);
                                parameterValueI32 = cmidi2_ump_get_midi2_cc_data(ump);
                            }
                            break;
                        case CMIDI2_STATUS_PER_NOTE_ACC:
                            if ((current_mapping_policy & AAP_PARAMETERS_MAPPING_POLICY_ACC) != 0 &&
                                    (current_mapping_policy & AAP_PARAMETERS_MAPPING_POLICY_SYSEX8) == 0)
                                parameterKey = cmidi2_ump_get_midi2_pnacc_note(ump);
                            // no break; go to case CMIDI2_STATUS_NRPN
                        case CMIDI2_STATUS_NRPN:
                            if ((current_mapping_policy & AAP_PARAMETERS_MAPPING_POLICY_ACC) != 0 &&
                                (current_mapping_policy & AAP_PARAMETERS_MAPPING_POLICY_SYSEX8) == 0) {
                                parameterIndex = cmidi2_ump_get_midi2_nrpn_msb(ump) * 0x80 +
                                        cmidi2_ump_get_midi2_nrpn_lsb(ump);
                                parameterValueI32 = cmidi2_ump_get_midi2_nrpn_data(ump);
                            }
                            break;
                        case CMIDI2_STATUS_PROGRAM:
                            // unless the plugin requires it to be passed directly, treat them as preset setter.
                            if ((current_mapping_policy & AAP_PARAMETERS_MAPPING_POLICY_PROGRAM) == 0) {
                                bool bankValid = (cmidi2_ump_get_midi2_program_options(ump) & CMIDI2_PROGRAM_CHANGE_OPTION_BANK_VALID) != 0;
                                auto bank = bankValid ?
                                        cmidi2_ump_get_midi2_program_bank_msb(ump) * 0x80 +
                                        cmidi2_ump_get_midi2_program_bank_lsb(ump) : 0;
                                presetIndex = cmidi2_ump_get_midi2_program_program(ump) + bank * 0x80;
                            }
                            break;
                    }
                    break;
            }
            if (presetIndex >= 0) {
                for (int i = 0; i < client->getInstanceCount(); i++) {
                    auto instance = client->getInstanceByIndex(i);
                    if (instance->getPluginInformation()->isInstrument())
                        instance->getStandardExtensions().setCurrentPresetIndex(presetIndex);
                }
            }
            // If a translated AAP parameter change message is detected, then output sysex8.
            if (parameterIndex < 0) {
                auto size = cmidi2_ump_get_message_size_bytes(ump);
                memcpy(translation_buffer + translatedIndex, ump, size);
                translatedIndex += size;
            }
            else {
                auto intBuf = (uint32_t*) ((uint8_t*) translation_buffer + translatedIndex);
                aapMidi2ParameterSysex8(intBuf, intBuf + 1, intBuf + 2, intBuf + 3,
                                        cmidi2_ump_get_group(ump), cmidi2_ump_get_channel(ump), parameterKey, parameterExtra, parameterIndex, *(float*) (void*) &parameterValueI32);
                translatedIndex += 16;
            }
        }
        memcpy(bytes + offset, translation_buffer, translatedIndex);
        return translatedIndex;
    }

    size_t AAPMidiProcessor::translateMidiBufferIfNeeded(uint8_t* bytes, size_t offset, size_t length) {
        if (length == 0)
            return 0;
        if (receiver_midi_protocol != CMIDI2_PROTOCOL_TYPE_MIDI2) {
            // It receives MIDI1 bytestream. We translate to MIDI2 UMPs.
            cmidi2_midi_conversion_context context;
            cmidi2_midi_conversion_context_initialize(&context);
            context.midi1 = bytes + offset;
            context.midi_protocol = CMIDI2_PROTOCOL_TYPE_MIDI2;
            context.midi1_num_bytes = length;
            context.ump = (cmidi2_ump*) translation_buffer;
            context.ump_num_bytes = sizeof(translation_buffer);
            context.group = 0;

            if (cmidi2_convert_midi1_to_ump(&context) != CMIDI2_CONVERSION_RESULT_OK) {
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "Failed to translate MIDI 1.0 inputs to MIDI 2.0 UMPs");
                return 0;
            }
            memcpy(bytes + offset, translation_buffer, context.ump_proceeded_bytes);
            return context.ump_proceeded_bytes;
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
        translated = runThroughMidi2UmpForMidiMapping(bytes, offset, length);
        if (translated > 0)
            length = translated;

        // it is 99.999... percent true since audio loop must have started before any MIDI events...
        if (last_aap_process_time.tv_sec > 0) {
            int64_t diff = (curtime.tv_sec - last_aap_process_time.tv_sec) * 1000000000 +
                        curtime.tv_nsec - last_aap_process_time.tv_nsec;
            auto nanosecondsPerCycle = (int64_t) (1.0 * aap_frame_size / sample_rate * 1000000000);
            actualTimestamp = (timestampInNanoseconds + diff) % nanosecondsPerCycle;
        }

        {
            const std::lock_guard<NanoSleepLock> lock{midi_buffer_mutex};

            auto dst8 = (uint8_t *) midi_input_buffer;
            auto dstMBH = (AAPMidiBufferHeader *) dst8;
            if (dst8 != nullptr) {
                uint32_t savedOffset = dstMBH->length;
                uint32_t currentOffset = savedOffset;
                int32_t tIter = 0;
                auto headerSize = sizeof(AAPMidiBufferHeader);
                for (int64_t ticks = actualTimestamp / (1000000000 / 31250);
                     ticks > 0; ticks -= 31250, tIter++) {
                    *(int32_t *) (dst8 + headerSize + currentOffset + tIter * 4) =
                            (int32_t) cmidi2_ump_jr_timestamp_direct(0,
                                                                     ticks > 31250 ? 31250 : ticks);
                }
                currentOffset += tIter * 4;
                memcpy(dst8 + headerSize + currentOffset, bytes + offset, length);
                currentOffset += length;

                if (savedOffset != dstMBH->length) {
                    // updated
                    memcpy(dst8 + headerSize, dst8 + headerSize + savedOffset, length);
                    dstMBH->length = currentOffset - savedOffset;
                } else
                    dstMBH->length = currentOffset;
            }
        }
    }
}
