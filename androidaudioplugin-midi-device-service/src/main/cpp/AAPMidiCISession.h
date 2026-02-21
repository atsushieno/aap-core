
#ifndef AAP_MIDI_DEVICE_SERVICE_AAPMIDICI_SESSION_H
#define AAP_MIDI_DEVICE_SERVICE_AAPMIDICI_SESSION_H

#include <cstdint>
#include <functional>
#include <memory>
#include <aap/core/host/plugin-instance.h>

namespace aap::midi {

/**
 * MIDI-CI session for an AAP instrument plugin instance.
 *
 * Provides AllCtrlList and CtrlMapList sourced from AAP plugin parameters, and
 * ProgramList sourced from AAP plugin presets.  This is the AAP-side analogue of
 * UapmdMidiCISessionImpl in the UAPMD project.
 *
 * Lifecycle
 * ---------
 *  1. Construct with a pointer to the instrument's PluginInstance (must outlive
 *     this object); PluginInformation and StandardExtensions are obtained from it.
 *  2. Call setupMidiCISession() once, supplying a sender callback that will
 *     write CI response bytes back to the connected host.
 *  3. Call interceptInput() for every incoming buffer, BEFORE the MIDI1→UMP
 *     translation step.  For MIDI 1 devices the CI layer sees the original SysEx
 *     byte stream; for UMP devices the bytes are already in UMP format.  Non-CI
 *     bytes are silently ignored and continue to flow to the plugin normally.
 *
 * MIDI output requirement
 * -----------------------
 * For CI responses to reach the connected host, the Android MidiDeviceService
 * must expose at least one output port (declared in midi_device_info.xml).  The
 * outputSender supplied to setupMidiCISession() should write to that port via
 * MidiReceiver.send() (forwarded from the Kotlin layer through JNI).
 */
class AAPMidiCISession {
public:
    // (data, offset, length, timestamp_ns) — matches midicci's MidiInputCallback signature
    using MidiSender = std::function<void(const uint8_t*, size_t, size_t, uint64_t)>;

    /**
     * @param instance  The instrument plugin instance.  Must outlive this object.
     *                  PluginInformation and StandardExtensions are obtained from it.
     * @param isUmp     true when the owning device service is a MidiUmpDeviceService
     *                  (full UMP byte-stream, no MIDI 1 conversion); false for a plain
     *                  MidiDeviceService (MIDI 1 SysEx byte-stream).
     */
    AAPMidiCISession(aap::PluginInstance* instance, bool isUmp);
    ~AAPMidiCISession();

    // Non-copyable, movable
    AAPMidiCISession(const AAPMidiCISession&) = delete;
    AAPMidiCISession& operator=(const AAPMidiCISession&) = delete;

    /**
     * Initialise the MIDI-CI session and populate AllCtrlList / CtrlMapList /
     * ProgramList from the plugin's parameters and presets.
     *
     * @param outputSender  Called whenever the CI session needs to send bytes
     *                      back to the host (e.g. property-exchange replies).
     */
    void setupMidiCISession(MidiSender outputSender);

    /**
     * Feed incoming bytes into the CI session.
     *
     * For MidiDeviceService (MIDI 1): pass the raw bytes BEFORE the MIDI1→UMP
     * translation step so that midicci sees the original SysEx stream (F0 7E…F7).
     *
     * For MidiUmpDeviceService (UMP): pass the raw UMP bytes directly — no
     * MIDI1→UMP conversion happens, so the call position is irrelevant, but
     * keeping it before the translation call is consistent and harmless.
     *
     * The CI session only acts on MIDI-CI SysEx / SysEx8 messages; all other
     * bytes are silently ignored.
     */
    void interceptInput(const uint8_t* data, size_t offset, size_t length,
                        uint64_t timestampInNanoseconds);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aap::midi

#endif // AAP_MIDI_DEVICE_SERVICE_AAPMIDICI_SESSION_H
