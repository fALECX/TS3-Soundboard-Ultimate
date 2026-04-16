#pragma once

#include <atomic>
#include <functional>
#include <mutex>

#include <QString>

#include "src/engine/upstream/SampleBuffer.h"
#include "src/engine/upstream/SampleProducerThread.h"
#include "src/engine/upstream/peakmeter.h"

class InputFile;
class SoundInfo;

class Sampler {
 public:
  enum state_e {
    eSILENT = 0,
    ePLAYING,
    ePAUSED,
    ePLAYING_PREVIEW,
  };

  Sampler();
  ~Sampler();
  void init();
  void shutdown();
  int fetchInputSamples(short* samples, int count, int channels, bool* finished);
  int fetchOutputSamples(short* samples, int count, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask);
  bool playFile(const SoundInfo& sound);
  bool playPreview(const SoundInfo& sound);
  void stopPlayback();
  void setVolumeLocal(int vol);
  void setVolumeRemote(int vol);
  void setLocalPlayback(bool enabled);
  void setMuteMyself(bool enabled);
  void pausePlayback();
  void unpausePlayback();
  state_e getState() const { return m_state; }

  std::function<void(bool, const QString&)> onStartPlaying;
  std::function<void()> onStopPlaying;
  std::function<void()> onPausePlaying;
  std::function<void()> onUnpausePlaying;

 private:
  void stopSoundInternal();
  bool playSoundInternal(const SoundInfo& sound, bool preview);
  void setVolumeDb(double decibel);
  int fetchSamples(SampleBuffer& sb, PeakMeter& pm, short* samples, int count, int channels, bool eraseConsumed, int ciLeft, int ciRight, bool overLeft, bool overRight);
  int findChannelId(unsigned int channel, const unsigned int* channelSpeakerArray, int count);

  SampleBuffer m_sbCapture;
  SampleBuffer m_sbPlayback;
  SampleProducerThread m_sampleProducerThread;
  InputFile* m_inputFile;
  PeakMeter m_peakMeterCapture;
  PeakMeter m_peakMeterPlayback;
  int m_volumeDivider;
  float m_volumeFactor;
  static const int volumeScaleExp = 12;
  double m_globalDbSettingLocal;
  double m_globalDbSettingRemote;
  double m_soundDbSetting;
  std::mutex m_mutex;
  std::atomic<state_e> m_state;
  bool m_localPlayback;
  bool m_muteMyself;
};
