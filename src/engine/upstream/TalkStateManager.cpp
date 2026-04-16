#include "src/engine/upstream/TalkStateManager.h"

#include <stdexcept>
#include <cstring>

#ifdef RPSU_ENABLE_TS3_ROUTING
namespace {
void runtimeLog(const QString& text) {
  if (ts3Functions.logMessage) {
    ts3Functions.logMessage(text.toUtf8().constData(), LogLevel_INFO, "RP Soundboard Ultimate", 0);
  }
}
}
#endif

namespace {
#define RETURN_ENUM_CASE(val) \
  case val:                   \
    return #val
}

const char* TalkStateManager::toString(talk_state_e ts) {
  switch (ts) {
    RETURN_ENUM_CASE(TS_INVALID);
    RETURN_ENUM_CASE(TS_PTT_WITHOUT_VA);
    RETURN_ENUM_CASE(TS_PTT_WITH_VA);
    RETURN_ENUM_CASE(TS_VOICE_ACTIVATION);
    RETURN_ENUM_CASE(TS_CONT_TRANS);
    default:
      throw std::logic_error("Invalid talk state");
  }
}

TalkStateManager::TalkStateManager()
    : previousTalkState(TS_INVALID),
      defaultTalkState(TS_INVALID),
      currentTalkState(TS_INVALID),
      activeServerId(0),
      playingServerId(0) {}

TalkStateManager::~TalkStateManager() = default;

void TalkStateManager::setStopPlaybackCallback(std::function<void()> callback) {
  stopPlaybackCallback_ = std::move(callback);
}

void TalkStateManager::onStartPlaying(bool preview, QString filename) {
  (void)filename;
  if (!preview) {
    playingServerId = activeServerId;
    setPlayTransMode();
  }
}

void TalkStateManager::onStopPlaying() {
  setTalkTransMode();
}

void TalkStateManager::onPauseSound() {
  setTalkTransMode();
}

void TalkStateManager::onUnpauseSound() {
  setPlayTransMode();
}

void TalkStateManager::setTalkTransMode() {
  if (previousTalkState == TS_INVALID) {
    return;
  }
  const talk_state_e ts = previousTalkState;
  previousTalkState = TS_INVALID;
  setTalkState(activeServerId, ts);
}

void TalkStateManager::setPlayTransMode() {
#ifdef RPSU_ENABLE_TS3_ROUTING
  talk_state_e s = getTalkState(activeServerId);
  if (defaultTalkState == TS_INVALID) {
    defaultTalkState = s;
  }
  if (s == TS_CONT_TRANS) {
    s = defaultTalkState;
  }
  if (s == TS_INVALID) {
    s = defaultTalkState;
  }
  if (s == TS_INVALID) {
    return;
  }

  previousTalkState = s;
  setContinuousTransmission(activeServerId);
#endif
}

void TalkStateManager::setActiveServerId(uint64 id) {
#ifdef RPSU_ENABLE_TS3_ROUTING
  if (id == activeServerId) {
    return;
  }

  const talk_state_e oldCurrentTS = currentTalkState;
  if (activeServerId != 0 && previousTalkState != TS_INVALID) {
    setTalkState(activeServerId, previousTalkState);
  }
  activeServerId = id;
  if (oldCurrentTS == TS_CONT_TRANS) {
    previousTalkState = id != 0 ? getTalkState(id) : TS_INVALID;
    if (previousTalkState != TS_INVALID) {
      setContinuousTransmission(id);
    } else if (stopPlaybackCallback_) {
      stopPlaybackCallback_();
    }
  }
#else
  activeServerId = id;
#endif
}

TalkStateManager::talk_state_e TalkStateManager::getTalkState(uint64 scHandlerID) {
#ifdef RPSU_ENABLE_TS3_ROUTING
  char* vadStr = nullptr;
  if (ts3Functions.getPreProcessorConfigValue(scHandlerID, "vad", &vadStr) != 0 || !vadStr) {
    return TS_INVALID;
  }
  const bool vad = strcmp(vadStr, "true") == 0;
  ts3Functions.freeMemory(vadStr);

  int input = 0;
  if (ts3Functions.getClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, &input) != 0) {
    return TS_INVALID;
  }
  const bool ptt = input == INPUT_DEACTIVATED;
  return ptt ? (vad ? TS_PTT_WITH_VA : TS_PTT_WITHOUT_VA) : (vad ? TS_VOICE_ACTIVATION : TS_CONT_TRANS);
#else
  (void)scHandlerID;
  return TS_INVALID;
#endif
}

bool TalkStateManager::setTalkState(uint64 scHandlerID, talk_state_e state) {
#ifdef RPSU_ENABLE_TS3_ROUTING
  if (scHandlerID == 0 || state == TS_INVALID) {
    runtimeLog(QStringLiteral("setTalkState rejected: server=%1 state=%2")
                   .arg(scHandlerID)
                   .arg(QString::fromLatin1(toString(state))));
    return false;
  }

  const bool va = state == TS_PTT_WITH_VA || state == TS_VOICE_ACTIVATION;
  const bool in = state == TS_CONT_TRANS || state == TS_VOICE_ACTIVATION;
  if (ts3Functions.setPreProcessorConfigValue(scHandlerID, "vad", va ? "true" : "false") != 0) {
    runtimeLog(QStringLiteral("setTalkState vad failed: server=%1 state=%2")
                   .arg(scHandlerID)
                   .arg(QString::fromLatin1(toString(state))));
    return false;
  }
  if (ts3Functions.setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, in ? INPUT_ACTIVE : INPUT_DEACTIVATED) != 0) {
    runtimeLog(QStringLiteral("setTalkState input failed: server=%1 state=%2")
                   .arg(scHandlerID)
                   .arg(QString::fromLatin1(toString(state))));
    return false;
  }

  ts3Functions.flushClientSelfUpdates(scHandlerID, nullptr);
  currentTalkState = state;
  runtimeLog(QStringLiteral("setTalkState applied: server=%1 state=%2")
                 .arg(scHandlerID)
                 .arg(QString::fromLatin1(toString(state))));
  return true;
#else
  (void)scHandlerID;
  (void)state;
  return false;
#endif
}

bool TalkStateManager::setContinuousTransmission(uint64 scHandlerID) {
  return setTalkState(scHandlerID, TS_CONT_TRANS);
}

void TalkStateManager::onClientStopsTalking() {
  if (currentTalkState == TS_CONT_TRANS &&
      (previousTalkState == TS_PTT_WITHOUT_VA || previousTalkState == TS_PTT_WITH_VA)) {
    setPlayTransMode();
  }
}
