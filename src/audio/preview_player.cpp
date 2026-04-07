#include "src/audio/preview_player.h"

#include <windows.h>
#include <mmsystem.h>

#include <QDir>

namespace rpsu {

namespace {

bool sendMciCommand(const QString& command, QString* response = nullptr) {
  wchar_t buffer[256] = {0};
  const MCIERROR error = mciSendStringW(
    reinterpret_cast<LPCWSTR>(command.utf16()),
    response ? buffer : nullptr,
    response ? 256 : 0,
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
}

bool PreviewPlayer::isPlaying(const QString& soundId) const {
  return !currentSoundId_.isEmpty() && currentSoundId_ == soundId;
}

}  // namespace rpsu
