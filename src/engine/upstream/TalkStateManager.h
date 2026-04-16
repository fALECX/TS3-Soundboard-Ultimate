#pragma once

#include <functional>

#include <QString>

#include "pluginsdk/include/ts3_functions.h"

#ifdef RPSU_ENABLE_TS3_ROUTING
extern struct TS3Functions ts3Functions;
#endif

class TalkStateManager {
 public:
  enum talk_state_e {
    TS_INVALID,
    TS_PTT_WITHOUT_VA,
    TS_PTT_WITH_VA,
    TS_VOICE_ACTIVATION,
    TS_CONT_TRANS,
  };

  TalkStateManager();
  ~TalkStateManager();

  void setStopPlaybackCallback(std::function<void()> callback);
  void setActiveServerId(uint64 id);
  void onClientStopsTalking();

  void onStartPlaying(bool preview, QString filename);
  void onStopPlaying();
  void onPauseSound();
  void onUnpauseSound();

 private:
  static const char* toString(talk_state_e ts);
  talk_state_e getTalkState(uint64 scHandlerID);
  bool setTalkState(uint64 scHandlerID, talk_state_e state);
  bool setContinuousTransmission(uint64 scHandlerID);
  void setTalkTransMode();
  void setPlayTransMode();

  talk_state_e previousTalkState;
  talk_state_e defaultTalkState;
  talk_state_e currentTalkState;
  uint64 activeServerId;
  uint64 playingServerId;
  std::function<void()> stopPlaybackCallback_;
};
