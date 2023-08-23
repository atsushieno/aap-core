#ifndef AAP_CORE_LOCALDEFINITIONS_H
#define AAP_CORE_LOCALDEFINITIONS_H

#define AAP_PUBLIC_API __attribute__((__visibility__("default")))
#define AAP_OPEN_CLASS

#include <aap/android-audio-plugin.h>
typedef aap_buffer_t AudioData;
#define AudioDataNumExtraChannels 2 // MIDI In and MIDI Out ports

#endif //AAP_CORE_LOCALDEFINITIONS_H
