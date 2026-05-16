#pragma once

#include <QString>

namespace rpsu {

class PreviewPlayer {
 public:
  ~PreviewPlayer();

  void setVolume(double volume);
  bool playFile(const QString& soundId, const QString& path, int* durationMs, QString* errorMessage);
  void stop();
  bool pause();
  bool resume();
  bool seekTo(int posMs);
  bool isPlaying(const QString& soundId) const;
  bool isActive() const;
  bool isPaused() const;
  int currentPositionMs() const;

 private:
  QString alias_ = QStringLiteral("rpsu_preview");
  QString currentSoundId_;
  int currentDurationMs_ = 0;
  double volume_ = 0.8;
  bool paused_ = false;
};

}  // namespace rpsu
