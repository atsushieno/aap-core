#ifndef AAP_CORE_LOCALDEFINITIONS_H
#define AAP_CORE_LOCALDEFINITIONS_H

#define AAP_OPEN_CLASS

#define AAP_MANAGER_MIDI_BUFFER_SIZE 65536
#define AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE 8192
#define AAP_MANAGER_LOG_TAG "AAPManager"
// Longer audio sources are rejected. They are decoded to float at their own channel count, so
// three minutes of 48kHz stereo take 66MB, and loading needs about as much again on top.
#define AAP_MANAGER_MAX_AUDIO_SOURCE_SECONDS 180

#endif //AAP_CORE_LOCALDEFINITIONS_H
