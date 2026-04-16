#pragma once

#include <QHash>
#include <QString>

#include "src/core/models.h"
#include "src/engine/upstream/Sampler.h"
#include "src/engine/upstream/SoundInfo.h"
#include "src/engine/upstream/TalkStateManager.h"

#ifdef RPSU_ENABLE_TS3_ROUTING
#include "src/plugin.h"
#endif

namespace rpsu {

class PlaybackEngine {
 public:
  PlaybackEngine();
  ~PlaybackEngine();

  void initialize();
  void shutdown();

  void setVolumeRemote(int value);
  void setVolumeLocal(int value);
  void setPlaybackLocal(bool enabled);
  void setMuteMyselfDuringPlayback(bool enabled);
  void stopPlayback();
  bool playSound(const SoundRecord& sound, const QString& soundsDir, QString* errorMessage);

  bool mixCaptured(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels);
  void mixPlayback(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask);

#ifdef RPSU_ENABLE_TS3_ROUTING
  void currentServerConnectionChanged(uint64 serverConnectionHandlerID);
  void connectStatusChanged(uint64 serverConnectionHandlerID, int newStatus);
  void updateClientEvent(uint64 serverConnectionHandlerID, anyID clientID);
  void talkStatusChanged(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID);
  bool hasActiveServer() const;
#else
  bool hasActiveServer() const { return false; }
#endif

  std::function<void(const QString&, int, bool)> onPreviewStatusChanged;

 private:
  SoundInfo toSoundInfo(const SoundRecord& sound, const QString& soundsDir) const;
  void updatePreviewStatus(const QString& title, int durationMs, bool playing) const;

  Sampler sampler_;
  TalkStateManager talkStateManager_;

#ifdef RPSU_ENABLE_TS3_ROUTING
  QHash<uint64, int> connectionStatuses_;
  QHash<uint64, int> inputHardwareStates_;
  uint64 activeServerConnectionHandlerId_ = 0;
#endif
};

}  // namespace rpsu
