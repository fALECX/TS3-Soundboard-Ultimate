#include "src/audio/mixer.h"

#include <QDir>

namespace rpsu {

void AudioMixer::setMasterVolume(double volume) {
  masterVolume_ = volume;
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
    if (!WavDecoder::decodeFile(QDir(soundsDir).filePath(sound.filename), decoded, errorMessage)) {
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

bool AudioMixer::isPlaying(const QString& soundId) const {
  for (const ActivePlayback& playback : active_) {
    if (playback.soundId == soundId) {
      return true;
    }
  }
  return false;
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

    if (qFuzzyIsNull(mixed)) {
      continue;
    }

    edited = true;
    for (int channel = 0; channel < channels; ++channel) {
      const int sampleIndex = frame * channels + channel;
      const int value = static_cast<int>(samples[sampleIndex] + mixed * 32767.0f);
      samples[sampleIndex] = static_cast<short>(qBound(-32768, value, 32767));
    }
  }

  return edited;
}

}  // namespace rpsu
