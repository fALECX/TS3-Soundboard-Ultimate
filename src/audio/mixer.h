#pragma once

#include <QMap>
#include <QString>
#include <QVector>

#include "src/audio/wav_decoder.h"
#include "src/core/models.h"

namespace rpsu {

class AudioMixer {
 public:
  void setMasterVolume(double volume);
  bool playSound(const SoundRecord& sound, const QString& soundsDir, QString* errorMessage);
  void stopSound(const QString& soundId);
  bool isPlaying(const QString& soundId) const;
  bool mixIntoCaptured(short* samples, int sampleCount, int channels);

 private:
  struct ActivePlayback {
    QString soundId;
    QVector<float> samples;
    int position = 0;
    float gain = 1.0f;
    bool loop = false;
  };

  double masterVolume_ = 0.8;
  QMap<QString, DecodedBuffer> cache_;
  QVector<ActivePlayback> active_;
};

}  // namespace rpsu
