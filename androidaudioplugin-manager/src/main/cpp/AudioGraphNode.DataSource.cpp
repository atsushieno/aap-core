#include "AudioGraphNode.h"
#include "AudioGraph.h"
#include <choc/audio/choc_AudioFileFormat_WAV.h>
#include <choc/audio/choc_AudioFileFormat_MP3.h>
#include <choc/audio/choc_AudioFileFormat_Ogg.h>
#include <choc/audio/choc_AudioFileFormat_FLAC.h>
#include <choc/audio/choc_SincInterpolator.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include "LocalDefinitions.h"


class SeekableByteBuffer : public std::streambuf {
public:
    SeekableByteBuffer(uint8_t* data, std::size_t size) {
        setg(reinterpret_cast<char*>(data), reinterpret_cast<char*>(data), reinterpret_cast<char*>(data) + size);
    }

    std::streampos seekoff(std::streamoff off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
        std::streamoff base;
        switch (dir) {
            case std::ios_base::beg: base = 0; break;
            case std::ios_base::cur: base = gptr() - eback(); break;
            case std::ios_base::end: base = egptr() - eback(); break;
            default: return {std::streamoff(-1)};
        }
        return seekpos(base + off, which);
    }

    // choc's WAV and FLAC readers seek to absolute positions (`seekg(pos)`), which std::streambuf
    // fails by default: WAV files could not be loaded at all.
    std::streampos seekpos(std::streampos pos, std::ios_base::openmode which) override {
        auto offset = static_cast<std::streamoff>(pos);
        if (offset < 0 || offset > egptr() - eback())
            return {std::streamoff(-1)};
        setg(eback(), eback() + offset, egptr());
        return pos;
    }
};

aap::AudioDataSourceNode::AudioDataSourceNode(aap::AudioGraph *ownerGraph) :
        AudioGraphNode(ownerGraph),
        audio_data(std::make_unique<aap::AudioBuffer>(ownerGraph->getChannelsInAudioBus(), ownerGraph->getFramesPerCallback())) {
}

bool aap::AudioDataSourceNode::shouldSkip() {
    // We do not filter out active. If !active and playing, then it consumes the stream.
    // Whether any data is left is checked in read(), under the lock.
    return !playing;
}

void aap::AudioDataSourceNode::processAudio(AudioBuffer *audioData, int32_t numFrames) {
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

int32_t aap::AudioDataSourceNode::read(AudioBuffer *dst, int32_t numFrames) {
    // read only if it is not locked: setAudioSource() may be replacing the data.
    std::unique_lock<NanoSleepLock> tryLock(data_source_mutex, std::try_to_lock);
    if (!tryLock.owns_lock() || !hasData() || numFrames <= 0)
        return 0;

    auto size = (uint32_t) std::min((int64_t) audio_data->audio.getNumFrames() - current_frame_offset,
                                    (int64_t) numFrames);

    if (!shouldConsumeButBypass()) {
        choc::buffer::FrameRange range{(uint32_t) current_frame_offset, current_frame_offset + size};
        choc::buffer::copyRemappingChannels(dst->audio.getStart(size),
                                            audio_data->audio.getFrameRange(range));
    }
    current_frame_offset += size;
    return size;
}

choc::audio::WAVAudioFileFormat<false> formatWav{};
choc::audio::MP3AudioFileFormat formatMp3{};
choc::audio::OggAudioFileFormat<false> formatOgg{};
choc::audio::FLACAudioFileFormat<false> formatFlac{};
choc::audio::AudioFileFormat* formats[] {&formatWav, &formatMp3, &formatOgg, &formatFlac};

// Decodes the whole data and resamples it to the graph's sample rate. Returns nullptr on failure.
static std::unique_ptr<aap::AudioBuffer> decodeAudio(choc::audio::AudioFileFormat* format,
                                                     uint8_t *data, int dataLength,
                                                     int32_t targetSampleRate,
                                                     const char *filename) {
    // The decoders throw on broken data, and the buffers may not fit in memory.
    try {
        SeekableByteBuffer buffer(data, dataLength);
        auto stream = std::make_shared<std::istream>(&buffer);
        auto reader = format->createReader(stream);
        if (!reader) {
            aap::a_log_f(AAP_LOG_LEVEL_WARN, AAP_MANAGER_LOG_TAG, "%s is not valid %s data", filename, format->getFileSuffixes().c_str());
            return nullptr;
        }
        auto props = reader->getProperties();
        if (props.numChannels == 0 || props.numFrames == 0 || props.sampleRate <= 0) {
            aap::a_log_f(AAP_LOG_LEVEL_WARN, AAP_MANAGER_LOG_TAG, "%s contains no audio", filename);
            return nullptr;
        }
        auto durationInSeconds = (double) props.numFrames / props.sampleRate;
        if (durationInSeconds > AAP_MANAGER_MAX_AUDIO_SOURCE_SECONDS) {
            aap::a_log_f(AAP_LOG_LEVEL_WARN, AAP_MANAGER_LOG_TAG, "%s is %.0f seconds long, and the limit is %d seconds",
                         filename, durationInSeconds, AAP_MANAGER_MAX_AUDIO_SOURCE_SECONDS);
            return nullptr;
        }
        auto decoded = std::make_unique<aap::AudioBuffer>((int32_t) props.numChannels, (int32_t) props.numFrames);
        if (!reader->readFrames(0, decoded->audio)) {
            aap::a_log_f(AAP_LOG_LEVEL_WARN, AAP_MANAGER_LOG_TAG, "Failed to decode %s", filename);
            return nullptr;
        }
        if (props.sampleRate == targetSampleRate)
            return decoded;

        auto targetFrames = (int32_t) std::llround(durationInSeconds * targetSampleRate);
        auto resampled = std::make_unique<aap::AudioBuffer>((int32_t) props.numChannels, targetFrames);
        choc::interpolation::sincInterpolate(resampled->audio, decoded->audio);
        return resampled;
    } catch (const std::exception& e) {
        aap::a_log_f(AAP_LOG_LEVEL_WARN, AAP_MANAGER_LOG_TAG, "Failed to load %s: %s", filename, e.what());
    } catch (...) {
        aap::a_log_f(AAP_LOG_LEVEL_WARN, AAP_MANAGER_LOG_TAG, "Failed to load %s", filename);
    }
    return nullptr;
}

bool aap::AudioDataSourceNode::setAudioSource(uint8_t *data, int dataLength, const char *filename) {
    if (!data || dataLength <= 0 || !filename)
        return false;

    // choc matches the suffixes case-sensitively, and they are all lower case.
    std::string name{filename};
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });

    for (auto format : formats) {
        if (!format->filenameSuffixMatches(name))
            continue;

        auto decoded = decodeAudio(format, data, dataLength, graph->getSampleRate(), filename);
        if (!decoded)
            return false;
        {
            const std::lock_guard <NanoSleepLock> lock{data_source_mutex};
            std::swap(audio_data, decoded);
            current_frame_offset = 0;
            playing = false;
        }
        // `decoded` now holds the previous data, which is released here, outside the lock.
        return true;
    }

    aap::a_log_f(AAP_LOG_LEVEL_WARN, AAP_MANAGER_LOG_TAG, "%s is not in a supported audio format", filename);
    return false;
}

aap::AudioDataSourceNode::~AudioDataSourceNode() {
    playing = false;
    active = false;
}

