
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wshadow"
#include <midicci/midicci.hpp>
#pragma clang diagnostic pop

#include <aap/ext/parameters.h>
#include <aap/ext/presets.h>
#include <aap/unstable/logging.h>

#include "AAPMidiCISession.h"

#define LOG_TAG "AAPMidiCISession"

using namespace midicci;
using namespace midicci::commonproperties;

namespace aap::midi {

// ---------------------------------------------------------------------------
// Pimpl implementation
// ---------------------------------------------------------------------------

class AAPMidiCISession::Impl {
    aap::PluginInstance* instance_;

    std::unique_ptr<midicci::musicdevice::MidiCISession> ci_session_{};
    std::vector<midicci::musicdevice::MidiInputCallback> ci_input_forwarders_{};

public:
    explicit Impl(aap::PluginInstance* instance)
        : instance_(instance) {}

    void setupMidiCISession(MidiSender outputSender);
    void interceptInput(umppi::UmpWordSpan words, uint64_t timestampInNanoseconds);
};

// ---------------------------------------------------------------------------
// Parameter → AllCtrlList / CtrlMapList helper
// (Mirrors setupParameterList() in UapmdMidiCISession.cpp)
// ---------------------------------------------------------------------------

static void setupParameterList(const std::string& controlType,
                                std::vector<MidiCIControl>& allCtrlList,
                                aap::PluginInstance* instance,
                                bool perNoteOnly,
                                MidiCIDevice& ciDevice) {
    auto& extensions = instance->getStandardExtensions();
    const int32_t count = extensions.getParameterCount();
    for (int32_t i = 0; i < count; i++) {
        aap_parameter_info_t param = extensions.getParameter(i);

        // Separate pass for normal vs per-note parameters.
        if (param.per_note_enabled != perNoteOnly)
            continue;

        // Map stable_id to the two-byte NRPN/PNAC controller address.
        const uint8_t msb = static_cast<uint8_t>(param.stable_id / 0x80);
        const uint8_t lsb = static_cast<uint8_t>(param.stable_id % 0x80);

        MidiCIControl ctrl{param.display_name, controlType, "",
                           std::vector<uint8_t>{msb, lsb}};

        if (param.path[0] != '\0')
            ctrl.paramPath = std::string(param.path);

        // Normalise a plain value to the [0, UINT32_MAX] range expected by MIDI-CI.
        const double range = param.max_value - param.min_value;
        auto plainToUint32 = [&](double plainValue) -> uint32_t {
            if (!(range > 0.0))
                return 0;
            double normalized = (plainValue - param.min_value) / range;
            normalized = std::clamp(normalized, 0.0, 1.0);
            return static_cast<uint32_t>(normalized * static_cast<double>(UINT32_MAX));
        };

        if (param.default_value != 0.0)
            ctrl.defaultValue = plainToUint32(param.default_value);

        // Build a CtrlMap entry for parameters that have enumerated values.
        const int32_t enumCount = extensions.getEnumerationCount(param.stable_id);
        if (enumCount > 0) {
            std::string mapId = std::to_string(param.stable_id);
            ctrl.ctrlMapId = mapId;

            std::vector<MidiCIControlMap> ctrlMapList;
            ctrlMapList.reserve(static_cast<size_t>(enumCount));
            for (int32_t j = 0; j < enumCount; j++) {
                aap_parameter_enum_t e = extensions.getEnumeration(param.stable_id, j);
                ctrlMapList.emplace_back(MidiCIControlMap{plainToUint32(e.value), e.name});
            }
            StandardPropertiesExtensions::setCtrlMapList(ciDevice, mapId, ctrlMapList);
        }

        allCtrlList.push_back(std::move(ctrl));
    }
}

// ---------------------------------------------------------------------------
// Impl::setupMidiCISession
// (Mirrors UapmdMidiCISessionImpl::setupMidiCISession())
// ---------------------------------------------------------------------------

void AAPMidiCISession::Impl::setupMidiCISession(MidiSender outputSender) {
    // --- Device configuration -------------------------------------------
    const auto* pluginInfo = instance_->getPluginInformation();
    auto& extensions = instance_->getStandardExtensions();
    const std::string deviceName = pluginInfo->getDisplayName();
    const std::string manufacturer = pluginInfo->getDeveloperName();
    const std::string version = pluginInfo->getVersion();

    midicci::MidiCIDeviceConfiguration ci_config{
        midicci::DEFAULT_RECEIVABLE_MAX_SYSEX_SIZE,
        midicci::DEFAULT_MAX_PROPERTY_CHUNK_SIZE,
        deviceName,
        0
    };
    ci_config.device_info.manufacturer = manufacturer;
    ci_config.device_info.model = deviceName;
    ci_config.device_info.version = version;

    auto device_info = ci_config.device_info;

    uint32_t muid{static_cast<uint32_t>(rand() & 0x7F7F7F7F)};

    // --- Transport wiring ------------------------------------------------
    auto input_listener_adder = [this](midicci::musicdevice::MidiInputCallback callback) {
        ci_input_forwarders_.push_back(std::move(callback));
    };

    auto sender = [outputSender = std::move(outputSender)](
                      umppi::UmpWordSpan words, uint64_t timestamp) {
        if (outputSender && !words.empty()) {
            outputSender(words, timestamp);
        }
    };

    midicci::musicdevice::MidiCISessionSource source{
        input_listener_adder,
        sender
    };

    ci_session_ = createMidiCiSession(source, muid, ci_config,
        [](const LogData& log) {
            auto msg = std::get_if<std::reference_wrapper<const Message>>(&log.data);
            if (msg)
                aap::a_log_f(AAP_LOG_LEVEL_DEBUG, LOG_TAG, "[CI %s] %s",
                             log.is_outgoing ? "OUT" : "IN",
                             msg->get().getLogMessage().c_str());
            else
                aap::a_log_f(AAP_LOG_LEVEL_DEBUG, LOG_TAG, "[CI %s] %s",
                             log.is_outgoing ? "OUT" : "IN",
                             std::get<std::string>(log.data).c_str());
        });

    auto& ciDevice = ci_session_->getDevice();

    // --- Register standard property metadata ----------------------------
    auto& hostProps = ciDevice.getPropertyHostFacade();
    hostProps.addMetadata(std::make_unique<CommonRulesPropertyMetadata>(
        StandardProperties::allCtrlListMetadata()));
    hostProps.addMetadata(std::make_unique<CommonRulesPropertyMetadata>(
        StandardProperties::chCtrlListMetadata()));
    hostProps.addMetadata(std::make_unique<CommonRulesPropertyMetadata>(
        StandardProperties::ctrlMapListMetadata()));
    hostProps.addMetadata(std::make_unique<CommonRulesPropertyMetadata>(
        StandardProperties::programListMetadata()));

    hostProps.updateCommonRulesDeviceInfo(device_info);

    // --- Populate AllCtrlList / CtrlMapList from AAP parameters ----------
    std::vector<MidiCIControl> allCtrlList{};

    // Normal (non-per-note) parameters → NRPN
    setupParameterList(MidiCIControlType::NRPN, allCtrlList, instance_,
                       /*perNoteOnly=*/false, ciDevice);

    // Per-note-enabled parameters → PNAC
    setupParameterList(MidiCIControlType::PNAC, allCtrlList, instance_,
                       /*perNoteOnly=*/true, ciDevice);

    StandardPropertiesExtensions::setAllCtrlList(ciDevice, allCtrlList);

    // --- Populate ProgramList from AAP presets ---------------------------
    // AAP aap_preset_t has an integer id and a name string; there is no
    // separate bank field, so bank is always treated as 0.  For preset IDs
    // >= 0x80 we use the upper bit of the bank-MSB byte (bit 6 set) to
    // signal that this byte carries the upper bits of the preset index,
    // matching the same encoding UAPMD uses for index-based presets.
    const int32_t presetCount = extensions.getPresetCount();
    std::vector<MidiCIProgram> programList{};
    programList.reserve(static_cast<size_t>(presetCount));
    for (int32_t i = 0; i < presetCount; i++) {
        aap_preset_t preset{};
        extensions.getPreset(i, preset);

        if (preset.id < (1 << 13)) {
            const auto msb  = static_cast<uint8_t>(preset.id >= 0x80
                                                   ? (preset.id / 0x80) | 0x40
                                                   : 0);
            const auto lsb  = static_cast<uint8_t>(0); // no bank concept in AAP
            const auto pc   = static_cast<uint8_t>(preset.id % 0x80);
            programList.push_back({preset.name, {msb, lsb, pc}});
        }
    }
    StandardPropertiesExtensions::setProgramList(ciDevice, programList);

    // --- MidiMessageReport handler ---------------------------------------
    // NOTE: AAP's StandardExtensions does not expose a "get current value"
    // call for parameters (values are only observable via MIDI2 NRPN output
    // from the plugin).  The handler therefore currently sends nothing.
    // Future work: track parameter changes emitted by the plugin's MIDI2
    // output port and replay them here.
    ciDevice.getMessenger().addMessageCallback([](const Message& req) {
        if (req.getType() == MessageType::MidiMessageReportInquiry) {
            aap::a_log_f(AAP_LOG_LEVEL_INFO, LOG_TAG,
                         "MidiMessageReportInquiry received — "
                         "parameter value dump not yet implemented for AAP.");
        }
    });

    aap::a_log_f(AAP_LOG_LEVEL_INFO, LOG_TAG,
                 "MIDI-CI session ready for plugin \"%s\" "
                 "(%d parameters, %d presets)",
                 deviceName.c_str(),
                 extensions.getParameterCount(),
                 presetCount);
}

// ---------------------------------------------------------------------------
// Impl::interceptInput
// ---------------------------------------------------------------------------

void AAPMidiCISession::Impl::interceptInput(umppi::UmpWordSpan words,
                                            uint64_t timestampInNanoseconds) {
    if (ci_input_forwarders_.empty() || words.empty())
        return;

    for (auto& forwarder : ci_input_forwarders_)
        forwarder(words, timestampInNanoseconds);
}

// ---------------------------------------------------------------------------
// AAPMidiCISession public interface (forwards to Impl)
// ---------------------------------------------------------------------------

AAPMidiCISession::AAPMidiCISession(aap::PluginInstance* instance)
    : impl_(std::make_unique<Impl>(instance)) {}

AAPMidiCISession::~AAPMidiCISession() = default;

void AAPMidiCISession::setupMidiCISession(MidiSender outputSender) {
    impl_->setupMidiCISession(std::move(outputSender));
}

void AAPMidiCISession::interceptInput(umppi::UmpWordSpan words,
                                      uint64_t timestampInNanoseconds) {
    impl_->interceptInput(words, timestampInNanoseconds);
}

} // namespace aap::midi
