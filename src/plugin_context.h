#pragma once

#include <QFileDialog>
#include <QDir>
#include <QHash>
#include <QMessageBox>
#include <QTimer>
#include <QtGlobal>

#include "pluginsdk/include/ts3_functions.h"
#include "src/audio/preview_player.h"
#include "src/core/storage.h"
#include "src/engine/playback_engine.h"
#include "src/core/youtube_service.h"
#include "src/ui/main_window.h"

#ifdef RPSU_ENABLE_TS3_ROUTING
#include "src/plugin.h"
#endif

namespace rpsu {

class PluginContext {
 public:
  static PluginContext& instance() {
    static PluginContext context;
    return context;
  }

  bool initialize() {
    state_ = storage_.loadState();
    playbackEngine_.onPreviewStatusChanged = [this](const QString& title, int durationMs, bool playing) {
      updatePreviewUi(title, durationMs, playing);
    };
    playbackEngine_.initialize();
    applyRuntimeConfig();
    storage_.saveState(state_);
    return true;
  }

  void shutdown() {
    playbackEngine_.shutdown();
    stopPlaybackRouting();
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
      window_->onYouTubePreview = [this](const YouTubeSearchResult& result, QString* errorMessage) {
        return previewYouTube(result, errorMessage);
      };
      window_->onFreesoundApiKeyChanged = [this](const QString& apiKey) {
        state_.config.freesoundApiKey = apiKey;
        storage_.saveState(state_);
      };
      window_->onVolumeRemoteChanged = [this](int value) {
        state_.config.volumeRemote = qBound(0, value, 100);
        playbackEngine_.setVolumeRemote(state_.config.volumeRemote);
        storage_.saveState(state_);
      };
      window_->onVolumeLocalChanged = [this](int value) {
        state_.config.volumeLocal = qBound(0, value, 100);
        preview_.setVolume(static_cast<double>(state_.config.volumeLocal) / 100.0);
        playbackEngine_.setVolumeLocal(state_.config.volumeLocal);
        storage_.saveState(state_);
      };
      window_->onPlaybackLocalChanged = [this](bool enabled) {
        state_.config.playbackLocal = enabled;
        playbackEngine_.setPlaybackLocal(enabled);
        if (!enabled) {
          stopPreview();
        }
        storage_.saveState(state_);
      };
      window_->onMuteMyselfDuringPlaybackChanged = [this](bool enabled) {
        state_.config.muteMyselfDuringPlayback = enabled;
        playbackEngine_.setMuteMyselfDuringPlayback(enabled);
        storage_.saveState(state_);
      };
      window_->onShowHotkeysOnButtonsChanged = [this](bool enabled) {
        state_.config.showHotkeysOnButtons = enabled;
        storage_.saveState(state_);
        refreshWindow();
      };
      window_->onGlobalHotkeysEnabledChanged = [this](bool enabled) {
        state_.config.globalHotkeysEnabled = enabled;
        storage_.saveState(state_);
        refreshWindow();
      };
      window_->onActiveBoardSizeChanged = [this](int rows, int cols) {
        resizeActiveBoard(rows, cols);
      };
      window_->onSoundEmojiChanged = [this](const QString& soundId, const QString& emoji) {
        changeSoundEmoji(soundId, emoji);
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
    if (!state_.config.globalHotkeysEnabled) {
      return;
    }

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
      bool started = false;
#ifdef RPSU_ENABLE_TS3_ROUTING
      started = playbackEngine_.playSound(sound, storage_.soundsDir(), &error);
#else
      const QString filePath = QDir(storage_.soundsDir()).filePath(sound.filename);
      int previewDurationMs = 0;
      QString previewError;
      started = preview_.playFile(sound.soundId, filePath, &previewDurationMs, &previewError);
      if (!started && error.isEmpty()) {
        error = previewError;
      }
      if (started) {
        updatePreviewUi(sound.displayName, previewDurationMs, true);
      }
#endif

      if (started) {
        sound.playCount += 1;
        sound.lastPlayedAt = nowIso();
        storage_.saveState(state_);
      }
      if (!started && !error.isEmpty() && window_) {
        window_->setPreviewStatus(QStringLiteral("Playback failed: %1").arg(error), 0, false);
      }
      return;
    }
  }

  bool mixCaptured(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels) {
    return playbackEngine_.mixCaptured(serverConnectionHandlerID, samples, sampleCount, channels);
  }

  bool mixPlayback(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask) {
    playbackEngine_.mixPlayback(serverConnectionHandlerID, samples, sampleCount, channels, channelSpeakerArray, channelFillMask);
    return true;
  }

#ifdef RPSU_ENABLE_TS3_ROUTING
  void currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
    playbackEngine_.currentServerConnectionChanged(serverConnectionHandlerID);
  }

  void connectStatusChanged(uint64 serverConnectionHandlerID, int newStatus) {
    playbackEngine_.connectStatusChanged(serverConnectionHandlerID, newStatus);
  }

  void updateClientEvent(uint64 serverConnectionHandlerID, anyID clientID) {
    playbackEngine_.updateClientEvent(serverConnectionHandlerID, clientID);
  }

  void talkStatusChanged(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID) {
    playbackEngine_.talkStatusChanged(serverConnectionHandlerID, status, isReceivedWhisper, clientID);
  }
#else
  void currentServerConnectionChanged(unsigned long long) {}
  void connectStatusChanged(unsigned long long, int) {}
  void updateClientEvent(unsigned long long, int) {}
  void talkStatusChanged(unsigned long long, int, int, int) {}
#endif

 private:
  PluginContext() {
    previewClearTimer_ = new QTimer();
    previewClearTimer_->setSingleShot(true);
    QObject::connect(previewClearTimer_, &QTimer::timeout, [this]() {
      updatePreviewUi(QString(), 0, false);
    });
  }

  ~PluginContext() {
    stopPlaybackRouting();
    if (previewClearTimer_) {
      previewClearTimer_->stop();
      delete previewClearTimer_;
      previewClearTimer_ = nullptr;
    }
  }

  void stopPreview() {
    playbackEngine_.stopPlayback();
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

  void applyRuntimeConfig() {
    playbackEngine_.setVolumeRemote(qBound(0, state_.config.volumeRemote, 100));
    playbackEngine_.setVolumeLocal(qBound(0, state_.config.volumeLocal, 100));
    playbackEngine_.setPlaybackLocal(state_.config.playbackLocal);
    playbackEngine_.setMuteMyselfDuringPlayback(state_.config.muteMyselfDuringPlayback);
    preview_.setVolume(static_cast<double>(qBound(0, state_.config.volumeLocal, 100)) / 100.0);
  }

  void resizeActiveBoard(int rows, int cols) {
    const int nextRows = qBound(1, rows, 50);
    const int nextCols = qBound(1, cols, 50);
    const int nextTotal = nextRows * nextCols;

    for (BoardRecord& board : state_.boards) {
      if (board.id != state_.activeBoardId) {
        continue;
      }

      if (board.rows == nextRows && board.cols == nextCols) {
        return;
      }

      const QVector<Cell> oldCells = board.cells;
      QVector<Cell> newCells;
      newCells.resize(nextTotal);

      const int copied = qMin(oldCells.size(), nextTotal);
      for (int index = 0; index < copied; ++index) {
        newCells[index] = oldCells[index];
      }

      for (int index = nextTotal; index < oldCells.size(); ++index) {
        if (!oldCells[index].soundId.isEmpty()) {
          board.unassignedSoundIds.push_back(oldCells[index].soundId);
        }
      }

      board.rows = nextRows;
      board.cols = nextCols;
      board.cells = newCells;
      storage_.saveState(state_);
      refreshWindow();
      return;
    }
  }

  void startPlaybackRouting() {}
  void reapplyPlaybackRouting() {}
  void stopPlaybackRouting() {}

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

  void changeSoundEmoji(const QString& soundId, const QString& emoji) {
    for (SoundRecord& sound : state_.library) {
      if (sound.soundId == soundId) {
        sound.icon = emoji;
        storage_.saveState(state_);
        refreshWindow();
        return;
      }
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

  bool previewYouTube(const YouTubeSearchResult& result, QString* errorMessage) {
    const QString tempDir = QDir::temp().filePath(QStringLiteral("rpsu_yt_preview"));
    QDir().mkpath(tempDir);

    QString filename;
    if (!youtube_.downloadAudio(result, tempDir, {}, &filename, errorMessage)) {
      return false;
    }

    const QString filePath = QDir(tempDir).filePath(filename);
    int durationMs = 0;
    QString playError;
    if (!preview_.playFile(QStringLiteral("yt_preview_") + result.id, filePath, &durationMs, &playError)) {
      if (errorMessage && errorMessage->isEmpty()) {
        *errorMessage = playError;
      }
      return false;
    }

    updatePreviewUi(result.title, durationMs, true);
    return true;
  }

  StorageManager storage_;
  AppState state_;
  PlaybackEngine playbackEngine_;
  PreviewPlayer preview_;
  YouTubeService youtube_;
  MainWindow* window_ = nullptr;
  QTimer* previewClearTimer_ = nullptr;
};

}  // namespace rpsu
