#include "src/audio/mixer.h"

#include <QDir>

#include "pluginsdk/include/teamspeak/public_definitions.h"
#include "src/audio/audio_decoder.h"

namespace rpsu {

void AudioMixer::setMasterVolume(double volume) {
  masterVolume_ = volume;
}

void AudioMixer::setMuteMyselfDuringPlayback(bool enabled) {
  muteMyselfDuringPlayback_ = enabled;
}

bool AudioMixer::playSound(const SoundRecord& sound, const QString& soundsDir, QString* errorMessage) {
  if (sound.soundId.isEmpty() || sound.filename.isEmpty()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Invalid sound record.");
    }
    return false;
  }

  if (sound.triggerMode == QStringLiteral("toggle") && isPlaying(sound.soundId)) {
    stopSound(sound.soundId);
    return true;
  }

  if (sound.playbackMode == QStringLiteral("interrupt") || sound.playbackMode == QStringLiteral("restart")) {
    stopSound(sound.soundId);
  }

    if (!cache_.contains(sound.filename)) {
    DecodedBuffer decoded;
    if (!AudioDecoder::decodeFile(QDir(soundsDir).filePath(sound.filename), decoded, errorMessage)) {
      return false;
    }
    cache_.insert(sound.filename, decoded);
  }

  const DecodedBuffer& decoded = cache_[sound.filename];
  ActivePlayback playback;
  playback.soundId = sound.soundId;
  playback.samples = decoded.monoSamples;
  playback.gain = static_cast<float>(sound.gain);
  playback.loop = sound.loop;

  const int trimStartFrame = qMax(0, (sound.trimStartMs * decoded.sampleRate) / 1000);
  const int trimEndFrame = qMax(0, (sound.trimEndMs * decoded.sampleRate) / 1000);
  playback.position = trimStartFrame;
  if (trimEndFrame > 0 && trimEndFrame < playback.samples.size()) {
    playback.samples.resize(trimEndFrame);
  }

  active_.push_back(playback);
  return true;
}

void AudioMixer::stopSound(const QString& soundId) {
  for (int index = active_.size() - 1; index >= 0; --index) {
    if (active_[index].soundId == soundId) {
      active_.remove(index);
    }
  }
}

void AudioMixer::stopAll() {
  active_.clear();
}

bool AudioMixer::isPlaying(const QString& soundId) const {
  for (const ActivePlayback& playback : active_) {
    if (playback.soundId == soundId) {
      return true;
    }
  }
  return false;
}

bool AudioMixer::hasActivePlayback() const {
  return !active_.isEmpty();
}

bool AudioMixer::mixIntoCaptured(short* samples, int sampleCount, int channels) {
  if (active_.isEmpty()) {
    return false;
  }

  bool edited = false;
  for (int frame = 0; frame < sampleCount; ++frame) {
    float mixed = 0.0f;
    for (int index = active_.size() - 1; index >= 0; --index) {
      ActivePlayback& playback = active_[index];
      if (playback.position >= playback.samples.size()) {
        if (playback.loop && !playback.samples.isEmpty()) {
          playback.position = 0;
        } else {
          active_.remove(index);
          continue;
        }
      }
      mixed += playback.samples[playback.position++] * playback.gain * static_cast<float>(masterVolume_);
    }

    for (int channel = 0; channel < channels; ++channel) {
      const int sampleIndex = frame * channels + channel;
      const int base = muteMyselfDuringPlayback_ ? 0 : samples[sampleIndex];
      const int value = static_cast<int>(base + mixed * 32767.0f);
      const short clamped = static_cast<short>(qBound(-32768, value, 32767));
      if (samples[sampleIndex] != clamped) {
        samples[sampleIndex] = clamped;
        edited = true;
      }
    }
  }

  return edited;
}

bool AudioMixer::mixIntoPlayback(short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask) {
  if (active_.isEmpty()) {
    return false;
  }

  bool edited = false;
  const unsigned int bitMaskLeft = SPEAKER_FRONT_LEFT | SPEAKER_HEADPHONES_LEFT;
  const unsigned int bitMaskRight = SPEAKER_FRONT_RIGHT | SPEAKER_HEADPHONES_RIGHT;
  const int leftIndex = [&]() {
    if (!channelSpeakerArray) {
      return 0;
    }
    for (int index = 0; index < channels; ++index) {
      if (channelSpeakerArray[index] & bitMaskLeft) {
        return index;
      }
    }
    return 0;
  }();
  const int rightIndex = [&]() {
    if (!channelSpeakerArray) {
      return qMin(1, channels - 1);
    }
    for (int index = 0; index < channels; ++index) {
      if (channelSpeakerArray[index] & bitMaskRight) {
        return index;
      }
    }
    return qMin(1, channels - 1);
  }();
  const bool fillLeft = !channelFillMask || ((*channelFillMask & bitMaskLeft) == 0);
  const bool fillRight = !channelFillMask || ((*channelFillMask & bitMaskRight) == 0);

  for (int frame = 0; frame < sampleCount; ++frame) {
    float mixed = 0.0f;
    for (int index = active_.size() - 1; index >= 0; --index) {
      ActivePlayback& playback = active_[index];
      if (playback.position >= playback.samples.size()) {
        if (playback.loop && !playback.samples.isEmpty()) {
          playback.position = 0;
        } else {
          active_.remove(index);
          continue;
        }
      }
      mixed += playback.samples[playback.position++] * playback.gain * static_cast<float>(masterVolume_);
    }

    auto writeChannel = [&](int channelIndex) {
      if (channelIndex < 0 || channelIndex >= channels) {
        return;
      }
      const int sampleIndex = frame * channels + channelIndex;
      const int base = samples[sampleIndex];
      const int value = static_cast<int>(base + mixed * 32767.0f);
      const short clamped = static_cast<short>(qBound(-32768, value, 32767));
      if (samples[sampleIndex] != clamped) {
        samples[sampleIndex] = clamped;
        edited = true;
      }
    };

    if (channels <= 1) {
      writeChannel(0);
    } else {
      if (fillLeft) {
        writeChannel(leftIndex);
      }
      if (fillRight) {
        writeChannel(rightIndex);
      }
    }
  }

  if (edited && channelFillMask) {
    *channelFillMask |= (bitMaskLeft | bitMaskRight);
  }
  return edited;
}

}  // namespace rpsu
