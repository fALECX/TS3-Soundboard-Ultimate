#pragma once

#include <QFileDialog>

#include "src/audio/mixer.h"
#include "src/core/storage.h"
#include "src/ui/main_window.h"

namespace rpsu {

class PluginContext {
 public:
  static PluginContext& instance() {
    static PluginContext context;
    return context;
  }

  bool initialize() {
    state_ = storage_.loadState();
    mixer_.setMasterVolume(state_.config.masterVolume);
    storage_.saveState(state_);
    return true;
  }

  void shutdown() {
    if (window_) {
      window_->close();
      delete window_;
      window_ = nullptr;
    }
  }

  const AppState& state() const { return state_; }

  void showWindow(QWidget* parent = nullptr) {
    if (!window_) {
      window_ = new MainWindow(parent);
      window_->onBoardSelected = [this](const QString& boardId) {
        state_.activeBoardId = boardId;
        storage_.saveState(state_);
        refreshWindow();
      };
      window_->onPlaySound = [this](const QString& soundId) {
        playSound(soundId);
      };
      window_->onImportSound = [this]() {
        importSound();
      };
      window_->onFreesoundApiKeyChanged = [this](const QString& apiKey) {
        state_.config.freesoundApiKey = apiKey;
        storage_.saveState(state_);
      };
    }

    refreshWindow();
    window_->show();
    window_->raise();
    window_->activateWindow();
  }

  void refreshWindow() {
    if (window_) {
      window_->setState(state_);
    }
  }

  QVector<HotkeyBinding> hotkeys() const {
    return buildHotkeyBindings(state_);
  }

  void playHotkey(const QString& keyword) {
    if (keyword.startsWith(QStringLiteral("board:"))) {
      state_.activeBoardId = keyword.mid(6);
      storage_.saveState(state_);
      refreshWindow();
      return;
    }

    if (!keyword.startsWith(QStringLiteral("cell:"))) {
      return;
    }

    const QStringList parts = keyword.split(QLatin1Char(':'));
    if (parts.size() != 3) {
      return;
    }

    const QString boardId = parts[1];
    const int cellIndex = parts[2].toInt();
    for (const BoardRecord& board : state_.boards) {
      if (board.id != boardId || cellIndex < 0 || cellIndex >= board.cells.size()) {
        continue;
      }
      playSound(board.cells[cellIndex].soundId);
      return;
    }
  }

  void playSound(const QString& soundId) {
    for (SoundRecord& sound : state_.library) {
      if (sound.soundId != soundId) {
        continue;
      }
      QString error;
      if (mixer_.playSound(sound, storage_.soundsDir(), &error)) {
        sound.playCount += 1;
        sound.lastPlayedAt = nowIso();
        storage_.saveState(state_);
      }
      return;
    }
  }

  bool mixCaptured(short* samples, int sampleCount, int channels) {
    return mixer_.mixIntoCaptured(samples, sampleCount, channels);
  }

 private:
  PluginContext() = default;

  void importSound() {
    if (!window_) {
      return;
    }

    const QString path = QFileDialog::getOpenFileName(
      window_,
      QStringLiteral("Import local sound"),
      QString(),
      QStringLiteral("Audio (*.wav *.mp3 *.ogg *.flac *.m4a)")
    );
    if (path.isEmpty()) {
      return;
    }
    storage_.importSoundFile(path, state_);
    storage_.saveState(state_);
    refreshWindow();
  }

  StorageManager storage_;
  AppState state_;
  AudioMixer mixer_;
  MainWindow* window_ = nullptr;
};

}  // namespace rpsu
