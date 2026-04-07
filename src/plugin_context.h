#pragma once

#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QTimer>

#include "src/audio/mixer.h"
#include "src/audio/preview_player.h"
#include "src/core/storage.h"
#include "src/core/youtube_service.h"
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
      window_->onStopPreview = [this]() {
        stopPreview();
      };
      window_->onImportSound = [this](int cellIndex) {
        importSound(cellIndex);
      };
      window_->onAssignSoundToCell = [this](const QString& soundId, int cellIndex) {
        assignSoundToCell(soundId, cellIndex);
      };
      window_->onYouTubeSearch = [this](const QString& query, int limit, QString* errorMessage) {
        return youtube_.search(query, limit, errorMessage);
      };
      window_->onYouTubeDownload = [this](const YouTubeSearchResult& result, QString* errorMessage) {
        return downloadYouTube(result, errorMessage);
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

      const QString filePath = QDir(storage_.soundsDir()).filePath(sound.filename);
      QString previewError;
      int previewDurationMs = 0;
      const bool previewStarted = preview_.playFile(sound.soundId, filePath, &previewDurationMs, &previewError);
      if (previewStarted) {
        updatePreviewUi(sound.displayName, previewDurationMs, preview_.isPlaying(sound.soundId));
        if (previewClearTimer_) {
          previewClearTimer_->stop();
          if (previewDurationMs > 0) {
            previewClearTimer_->start(previewDurationMs + 250);
          }
        }
      } else if (window_ && !previewError.isEmpty()) {
        window_->setPreviewStatus(QStringLiteral("Preview failed: %1").arg(previewError), 0, false);
      }

      QString error;
      if (mixer_.playSound(sound, storage_.soundsDir(), &error) || previewStarted) {
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
  PluginContext() {
    previewClearTimer_ = new QTimer();
    previewClearTimer_->setSingleShot(true);
    QObject::connect(previewClearTimer_, &QTimer::timeout, [this]() {
      updatePreviewUi(QString(), 0, false);
    });
  }

  ~PluginContext() {
    if (previewClearTimer_) {
      previewClearTimer_->stop();
      delete previewClearTimer_;
      previewClearTimer_ = nullptr;
    }
  }

  void stopPreview() {
    preview_.stop();
    if (previewClearTimer_) {
      previewClearTimer_->stop();
    }
    updatePreviewUi(QString(), 0, false);
  }

  void updatePreviewUi(const QString& title, int durationMs, bool playing) {
    if (window_) {
      window_->setPreviewStatus(title, durationMs, playing);
    }
  }

  void assignSoundToCell(const QString& soundId, int cellIndex) {
    if (cellIndex < 0) {
      return;
    }

    for (BoardRecord& board : state_.boards) {
      if (board.id != state_.activeBoardId || cellIndex >= board.cells.size()) {
        continue;
      }

      board.cells[cellIndex].soundId = soundId;
      board.unassignedSoundIds.removeAll(soundId);
      storage_.saveState(state_);
      refreshWindow();
      return;
    }
  }

  void importSound(int cellIndex) {
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
    const QString soundId = storage_.importSoundFile(path, state_);
    if (!soundId.isEmpty()) {
      assignSoundToCell(soundId, cellIndex);
    } else {
      storage_.saveState(state_);
      refreshWindow();
    }
  }

  QString downloadYouTube(const YouTubeSearchResult& result, QString* errorMessage) {
    QStringList existingNames;
    for (const SoundRecord& sound : state_.library) {
      existingNames.push_back(sound.filename);
    }

    QString filename;
    if (!youtube_.downloadAudio(result, storage_.soundsDir(), existingNames, &filename, errorMessage)) {
      return QString();
    }

    SoundRecord sound = createSoundRecord(filename);
    sound.displayName = result.title;
    sound.sourceType = QStringLiteral("youtube");
    sound.sourceUrl = result.url;
    sound.tags = QStringList{ QStringLiteral("youtube") };
    state_.library.push_back(sound);
    storage_.saveState(state_);
    refreshWindow();
    return sound.soundId;
  }

  StorageManager storage_;
  AppState state_;
  AudioMixer mixer_;
  PreviewPlayer preview_;
  YouTubeService youtube_;
  MainWindow* window_ = nullptr;
  QTimer* previewClearTimer_ = nullptr;
};

}  // namespace rpsu
