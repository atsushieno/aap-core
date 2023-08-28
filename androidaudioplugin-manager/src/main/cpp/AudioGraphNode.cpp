#include "AudioGraphNode.h"
#include "AudioGraph.h"
#include <audio/choc_AudioFileFormat_Wav.h>
#include <audio/choc_AudioFileFormat_MP3.h>
#include <audio/choc_AudioFileFormat_Ogg.h>
#include <audio/choc_AudioFileFormat_FLAC.h>

void aap::AudioDeviceInputNode::processAudio(AudioData *audioData, int32_t numFrames) {
    // copy current audio input data into `audioData`
    getDevice()->read(audioData, consumer_position, numFrames);
    // TODO: adjust ring buffer offset, not just simple adder.
    consumer_position += numFrames;
}

void aap::AudioDeviceInputNode::start() {
    if (!shouldSkip())
        getDevice()->startCallback();
}

void aap::AudioDeviceInputNode::pause() {
    if (!shouldSkip())
        getDevice()->stopCallback();
}

void aap::AudioDeviceInputNode::setPermissionGranted() {
    permission_granted = true;
}

bool aap::AudioDeviceInputNode::shouldSkip() {
    return getDevice()->isPermissionRequired() && !permission_granted;
}

//--------

void aap::AudioDeviceOutputNode::processAudio(AudioData *audioData, int32_t numFrames) {
    // copy `audioData` into current audio output buffer
    getDevice()->write(audioData, consumer_position, numFrames);
    // TODO: adjust ring buffer offset, not just simple adder.
    consumer_position += numFrames;
}

void aap::AudioDeviceOutputNode::start() {
    getDevice()->startCallback();
}

void aap::AudioDeviceOutputNode::pause() {
    getDevice()->stopCallback();
}

//--------

class SeekableByteBuffer : public std::streambuf {
public:
    SeekableByteBuffer(uint8_t* data, std::size_t size) {
        setg(reinterpret_cast<char*>(data), reinterpret_cast<char*>(data), reinterpret_cast<char*>(data) + size);
    }

    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
        switch (dir) {
            case std::ios_base::beg: setg(eback(), eback() + off, egptr()); break;
            case std::ios_base::cur: gbump((int) off); break;
            case std::ios_base::end: setg(eback(), egptr() + off, egptr()); break;
        }
        return gptr() - eback();
    }
};

aap::AudioDataSourceNode::AudioDataSourceNode(aap::AudioGraph *ownerGraph) :
        AudioGraphNode(ownerGraph),
        audio_data(ownerGraph->getChannelsInAudioBus(), ownerGraph->getFramesPerCallback()) {
}

bool aap::AudioDataSourceNode::shouldSkip() {
    // We do not filter out active. If !active and playing, then it consumes the stream
    return !hasData() || !playing;
}

void aap::AudioDataSourceNode::processAudio(AudioData *audioData, int32_t numFrames) {
    // It ignores errors or empty buffers
    read(audioData, numFrames);
}

void aap::AudioDataSourceNode::start() {
    active = true;
}

void aap::AudioDataSourceNode::pause() {
    active = false;
}

void aap::AudioDataSourceNode::setPlaying(bool newPlayingState) {
    // if it was already playing, reset current position.
    if (playing)
        current_frame_offset = 0;
    playing = newPlayingState;
}

int32_t aap::AudioDataSourceNode::read(AudioData *dst, int32_t numFrames) {

    uint32_t size = std::min(audio_data.audio.getNumFrames() - current_frame_offset,
                             (uint32_t) numFrames);
    if (size <= 0)
        return 0;

    if (shouldConsumeButBypass()) {
        current_frame_offset += size;
        return size;
    }

    // read only if it is not locked.
    if (std::unique_lock<NanoSleepLock> tryLock(data_source_mutex, std::try_to_lock); tryLock.owns_lock()) {

        choc::buffer::FrameRange range{(uint32_t) current_frame_offset, current_frame_offset + size};
        choc::buffer::copyRemappingChannels(dst->audio.getStart(size),
                                            audio_data.audio.getFrameRange(range));

        aap::a_log_f(AAP_LOG_LEVEL_DEBUG, AAP_MANAGER_LOG_TAG,
                     "AudioDataSourceNode::read() consumed %d frames. Remaining: %d",
                     numFrames,
                     audio_data.audio.getNumFrames() - current_frame_offset);
        current_frame_offset += size;
        return size;
    }
    else
        return 0;
}

choc::audio::WAVAudioFileFormat<false> formatWav{};
choc::audio::MP3AudioFileFormat formatMp3{};
choc::audio::OggAudioFileFormat<false> formatOgg{};
choc::audio::FLACAudioFileFormat<false> formatFlac{};
choc::audio::AudioFileFormat* formats[] {&formatWav, &formatMp3, &formatOgg, &formatFlac};

bool aap::AudioDataSourceNode::setAudioSource(uint8_t *data, int dataLength, const char *filename) {
    const std::lock_guard <NanoSleepLock> lock{data_source_mutex};

    for (auto format : formats) {
        if (format->filenameSuffixMatches(filename)) {
            SeekableByteBuffer buffer(data, dataLength);
            auto stream = std::make_shared<std::istream>(&buffer);
            auto reader = format->createReader(stream);
            reader->loadFileContent();
            auto props = reader->getProperties();
            audio_data = AudioData{(int32_t) props.numChannels, (int32_t) props.numFrames};
            reader->readFrames(0, audio_data.audio);
            return true;
        }
    }

    return false;
}

