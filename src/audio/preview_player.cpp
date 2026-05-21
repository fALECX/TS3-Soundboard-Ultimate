#include "src/audio/preview_player.h"

#include <windows.h>
#include <mmsystem.h>

#include <QDir>
#include <QtGlobal>

namespace rpsu {

namespace {

constexpr int kMciBufSize = 256;
constexpr int kMciVolumeScale = 1000;

bool sendMciCommand(const QString& command, QString* response = nullptr) {
  wchar_t buffer[kMciBufSize] = {0};
  const MCIERROR error = mciSendStringW(
    reinterpret_cast<LPCWSTR>(command.utf16()),
    response ? buffer : nullptr,
    response ? kMciBufSize : 0,
    NULL
  );

  if (error != 0) {
    return false;
  }

  if (response) {
    *response = QString::fromWCharArray(buffer);
  }
  return true;
}

}  // namespace

PreviewPlayer::~PreviewPlayer() {
  stop();
}

void PreviewPlayer::setVolume(double volume) {
  volume_ = qBound(0.0, volume, 1.0);
  if (!currentSoundId_.isEmpty()) {
    const int scaled = qBound(0, static_cast<int>(volume_ * kMciVolumeScale), kMciVolumeScale);
    sendMciCommand(QStringLiteral("setaudio %1 volume to %2").arg(alias_).arg(scaled));
  }
}

bool PreviewPlayer::playFile(const QString& soundId, const QString& path, int* durationMs, QString* errorMessage) {
  if (isPlaying(soundId)) {
    stop();
    if (durationMs) {
      *durationMs = 0;
    }
    return true;
  }

  stop();

  const QString normalizedPath = QDir::toNativeSeparators(path);
  const QString openCommand = QStringLiteral("open \"%1\" alias %2").arg(normalizedPath, alias_);
  if (!sendMciCommand(openCommand)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Could not open the sound for local preview.");
    }
    return false;
  }

  sendMciCommand(QStringLiteral("set %1 time format milliseconds").arg(alias_));
  const int scaledVolume = qBound(0, static_cast<int>(volume_ * kMciVolumeScale), kMciVolumeScale);
  sendMciCommand(QStringLiteral("setaudio %1 volume to %2").arg(alias_).arg(scaledVolume));

  QString durationValue;
  if (sendMciCommand(QStringLiteral("status %1 length").arg(alias_), &durationValue)) {
    currentDurationMs_ = durationValue.toInt();
  } else {
    currentDurationMs_ = 0;
  }

  if (!sendMciCommand(QStringLiteral("play %1").arg(alias_))) {
    sendMciCommand(QStringLiteral("close %1").arg(alias_));
    currentDurationMs_ = 0;
    if (errorMessage) {
      *errorMessage = QStringLiteral("Could not play the sound preview.");
    }
    return false;
  }

  currentSoundId_ = soundId;
  if (durationMs) {
    *durationMs = currentDurationMs_;
  }
  return true;
}

void PreviewPlayer::stop() {
  sendMciCommand(QStringLiteral("stop %1").arg(alias_));
  sendMciCommand(QStringLiteral("close %1").arg(alias_));
  currentSoundId_.clear();
  currentDurationMs_ = 0;
  paused_ = false;
}

bool PreviewPlayer::pause() {
  if (currentSoundId_.isEmpty() || paused_) return false;
  if (!sendMciCommand(QStringLiteral("pause %1").arg(alias_))) return false;
  paused_ = true;
  return true;
}

bool PreviewPlayer::resume() {
  if (currentSoundId_.isEmpty() || !paused_) return false;
  if (!sendMciCommand(QStringLiteral("resume %1").arg(alias_))) return false;
  paused_ = false;
  return true;
}

bool PreviewPlayer::seekTo(int posMs) {
  if (currentSoundId_.isEmpty()) return false;
  // play from new position (also exits paused state — seeking always resumes)
  const bool ok = sendMciCommand(QStringLiteral("play %1 from %2").arg(alias_).arg(posMs));
  if (ok) paused_ = false;
  return ok;
}

bool PreviewPlayer::isPlaying(const QString& soundId) const {
  return !currentSoundId_.isEmpty() && currentSoundId_ == soundId;
}

bool PreviewPlayer::isActive() const {
  return !currentSoundId_.isEmpty();
}

bool PreviewPlayer::isPaused() const {
  return paused_;
}

int PreviewPlayer::currentPositionMs() const {
  if (currentSoundId_.isEmpty()) return -1;
  QString pos;
  if (sendMciCommand(QStringLiteral("status %1 position").arg(alias_), &pos)) {
    return pos.trimmed().toInt();
  }
  return -1;
}

}  // namespace rpsu
