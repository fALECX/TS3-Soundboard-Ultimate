#include "src/engine/playback_engine.h"

#include <QDir>
#include <QFileInfo>

#include <cmath>

namespace rpsu {

#ifdef RPSU_ENABLE_TS3_ROUTING
namespace {
void runtimeLog(const QString& text) {
  if (ts3Functions.logMessage) {
    ts3Functions.logMessage(text.toUtf8().constData(), LogLevel_INFO, "RP Soundboard Ultimate", 0);
  }
}
}
#endif

PlaybackEngine::PlaybackEngine() {
  sampler_.onStartPlaying = [this](bool, const QString& filename) {
    updatePreviewStatus(QFileInfo(filename).completeBaseName(), 0, true);
    talkStateManager_.onStartPlaying(false, filename);
  };
  sampler_.onStopPlaying = [this]() {
    updatePreviewStatus(QString(), 0, false);
    talkStateManager_.onStopPlaying();
  };
  sampler_.onPausePlaying = [this]() { talkStateManager_.onPauseSound(); };
  sampler_.onUnpausePlaying = [this]() { talkStateManager_.onUnpauseSound(); };

  talkStateManager_.setStopPlaybackCallback([this]() { stopPlayback(); });
}

PlaybackEngine::~PlaybackEngine() {
  shutdown();
}

void PlaybackEngine::initialize() {
  sampler_.init();
}

void PlaybackEngine::shutdown() {
  sampler_.shutdown();
}

void PlaybackEngine::setVolumeRemote(int value) {
  sampler_.setVolumeRemote(value);
}

void PlaybackEngine::setVolumeLocal(int value) {
  sampler_.setVolumeLocal(value);
}

void PlaybackEngine::setPlaybackLocal(bool enabled) {
  sampler_.setLocalPlayback(enabled);
}

void PlaybackEngine::setMuteMyselfDuringPlayback(bool enabled) {
  sampler_.setMuteMyself(enabled);
}

void PlaybackEngine::stopPlayback() {
  sampler_.stopPlayback();
}

SoundInfo PlaybackEngine::toSoundInfo(const SoundRecord& sound, const QString& soundsDir) const {
  SoundInfo info;
  info.filename = QDir(soundsDir).filePath(sound.filename);
  info.customText = sound.displayName;
  info.volume = sound.gain > 0.0 ? static_cast<int>(std::round(10.0 * std::log10(sound.gain))) : -100;
  info.cropEnabled = sound.trimStartMs > 0 || sound.trimEndMs > 0;
  info.cropStartValue = sound.trimStartMs;
  info.cropStartUnit = 0;
  info.cropStopAfterAt = sound.trimEndMs > 0 ? 1 : 0;
  info.cropStopValue = sound.trimEndMs;
  info.cropStopUnit = 0;
  return info;
}

bool PlaybackEngine::playSound(const SoundRecord& sound, const QString& soundsDir, QString* errorMessage) {
#ifdef RPSU_ENABLE_TS3_ROUTING
  if (!hasActiveServer()) {
    runtimeLog(QStringLiteral("playSound blocked: no active server for %1").arg(sound.displayName));
    if (errorMessage) {
      *errorMessage = QStringLiteral("No active TeamSpeak input route is available.");
    }
    return false;
  }
  runtimeLog(QStringLiteral("playSound start: server=%1 sound=%2 file=%3")
                 .arg(activeServerConnectionHandlerId_)
                 .arg(sound.displayName, sound.filename));
#endif
  const SoundInfo info = toSoundInfo(sound, soundsDir);
  if (!sampler_.playFile(info)) {
#ifdef RPSU_ENABLE_TS3_ROUTING
    runtimeLog(QStringLiteral("playSound failed in sampler for %1").arg(sound.displayName));
#endif
    if (errorMessage) {
      *errorMessage = QStringLiteral("Upstream playback engine could not start the sound.");
    }
    return false;
  }
#ifdef RPSU_ENABLE_TS3_ROUTING
  runtimeLog(QStringLiteral("playSound accepted by sampler for %1").arg(sound.displayName));
#endif
  return true;
}

bool PlaybackEngine::mixCaptured(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels) {
#ifdef RPSU_ENABLE_TS3_ROUTING
  if (serverConnectionHandlerID != activeServerConnectionHandlerId_) {
    return false;
  }
#else
  (void)serverConnectionHandlerID;
#endif
  const int written = sampler_.fetchInputSamples(samples, sampleCount, channels, nullptr);
#ifdef RPSU_ENABLE_TS3_ROUTING
  static int captureLogBudget = 8;
  if (captureLogBudget > 0 && written > 0) {
    runtimeLog(QStringLiteral("mixCaptured wrote=%1 samples=%2 channels=%3 server=%4")
                   .arg(written)
                   .arg(sampleCount)
                   .arg(channels)
                   .arg(serverConnectionHandlerID));
    --captureLogBudget;
  }
#endif
  return written > 0;
}

void PlaybackEngine::mixPlayback(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask) {
#ifdef RPSU_ENABLE_TS3_ROUTING
  if (serverConnectionHandlerID != activeServerConnectionHandlerId_) {
    return;
  }
#else
  (void)serverConnectionHandlerID;
#endif
  sampler_.fetchOutputSamples(samples, sampleCount, channels, channelSpeakerArray, channelFillMask);
}

#ifdef RPSU_ENABLE_TS3_ROUTING
void PlaybackEngine::currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
  activeServerConnectionHandlerId_ = serverConnectionHandlerID;
  talkStateManager_.setActiveServerId(serverConnectionHandlerID);
}

void PlaybackEngine::connectStatusChanged(uint64 serverConnectionHandlerID, int newStatus) {
  if (newStatus == STATUS_DISCONNECTED) {
    inputHardwareStates_.remove(serverConnectionHandlerID);
    connectionStatuses_.remove(serverConnectionHandlerID);
  } else {
    connectionStatuses_.insert(serverConnectionHandlerID, newStatus);
  }

  if (serverConnectionHandlerID == activeServerConnectionHandlerId_ && newStatus == STATUS_DISCONNECTED) {
    stopPlayback();
    activeServerConnectionHandlerId_ = 0;
    talkStateManager_.setActiveServerId(0);
  } else if (newStatus == STATUS_CONNECTION_ESTABLISHED &&
             (activeServerConnectionHandlerId_ == 0 ||
              activeServerConnectionHandlerId_ == serverConnectionHandlerID)) {
    activeServerConnectionHandlerId_ = serverConnectionHandlerID;
    talkStateManager_.setActiveServerId(serverConnectionHandlerID);
  }
}

void PlaybackEngine::updateClientEvent(uint64 serverConnectionHandlerID, anyID clientID) {
  if (!ts3Functions.getClientID || !ts3Functions.getClientSelfVariableAsInt) {
    return;
  }

  anyID myId = 0;
  if (ts3Functions.getClientID(serverConnectionHandlerID, &myId) != 0 || myId != clientID) {
    return;
  }

  int inputState = 0;
  if (ts3Functions.getClientSelfVariableAsInt(serverConnectionHandlerID, CLIENT_INPUT_HARDWARE, &inputState) != 0) {
    return;
  }

  const int oldInputState = inputHardwareStates_.value(serverConnectionHandlerID, 0);
  inputHardwareStates_.insert(serverConnectionHandlerID, inputState);
  if (inputState && !oldInputState) {
    activeServerConnectionHandlerId_ = serverConnectionHandlerID;
    talkStateManager_.setActiveServerId(serverConnectionHandlerID);
  }
}

void PlaybackEngine::talkStatusChanged(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID) {
  anyID myId = 0;
  if (!ts3Functions.getClientID || ts3Functions.getClientID(serverConnectionHandlerID, &myId) != 0) {
    return;
  }
  if (clientID == myId && status == 0 && isReceivedWhisper == 0) {
    talkStateManager_.onClientStopsTalking();
  }
}

bool PlaybackEngine::hasActiveServer() const {
  return activeServerConnectionHandlerId_ != 0;
}
#endif

void PlaybackEngine::updatePreviewStatus(const QString& title, int durationMs, bool playing) const {
  if (onPreviewStatusChanged) {
    onPreviewStatusChanged(title, durationMs, playing);
  }
}

}  // namespace rpsu
