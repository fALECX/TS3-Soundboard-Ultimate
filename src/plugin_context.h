#pragma once

#include <atomic>

#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
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
      window_->onCreateBoard = [this](const QString& name, int rows, int cols) {
        createBoard(name, rows, cols);
      };
      window_->onPlaySound = [this](const QString& soundId) {
        playSound(soundId);
      };
      window_->onStopPreview = [this]() {
        stopPreview();
      };
      window_->onPausePreview = [this]() {
        pausePreview();
      };
      window_->onSeekPreview = [this](int posMs) {
        seekPreview(posMs);
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
      window_->onYouTubePreview = [this](const YouTubeSearchResult& result, QString* errorMessage) {
        return previewYouTube(result, errorMessage);
      };
      window_->onYouTubeDownload = [this](const YouTubeSearchResult& result, QString* errorMessage, std::atomic<bool>* cancelFlag, std::atomic<int>* progressPct) {
        return downloadYouTube(result, errorMessage, cancelFlag, progressPct);
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
      window_->onCellEmojiChanged = [this](int cellIndex, const QString& emoji) {
        changeCellEmoji(cellIndex, emoji);
      };
      window_->onSoundRenamed = [this](const QString& soundId, const QString& displayName) {
        renameSoundDisplay(soundId, displayName);
      };
      window_->onCellHotkeyChanged = [this](int cellIndex, const QString& hotkey) {
        setCellHotkey(cellIndex, hotkey);
      };
      window_->onDeleteBoard = [this](const QString& boardId) {
        deleteBoard(boardId);
      };
      window_->onRenameBoard = [this](const QString& boardId, const QString& newName) {
        renameBoard(boardId, newName);
      };
      window_->onDeleteSound = [this](const QString& soundId) {
        deleteSound(soundId);
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

  void reloadState() {
    state_ = storage_.loadState();
    applyRuntimeConfig();
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
        currentPreviewTitle_ = sound.displayName;
        currentPreviewDurationMs_ = previewDurationMs;
        if (positionPollTimer_) positionPollTimer_->start();
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
    positionPollTimer_ = new QTimer();
    positionPollTimer_->setInterval(50);
    QObject::connect(positionPollTimer_, &QTimer::timeout, [this]() {
      if (window_ && preview_.isActive()) {
        const int pos = preview_.currentPositionMs();
        if (pos >= 0) window_->updatePreviewProgress(pos);
      }
    });
  }

  ~PluginContext() {
    if (positionPollTimer_) {
      positionPollTimer_->stop();
      delete positionPollTimer_;
      positionPollTimer_ = nullptr;
    }
    if (previewClearTimer_) {
      previewClearTimer_->stop();
      delete previewClearTimer_;
      previewClearTimer_ = nullptr;
    }
  }

  void stopPreview() {
    playbackEngine_.stopPlayback();
    preview_.stop();
    currentPreviewTitle_.clear();
    currentPreviewDurationMs_ = 0;
    if (positionPollTimer_) positionPollTimer_->stop();
    if (previewClearTimer_) previewClearTimer_->stop();
    updatePreviewUi(QString(), 0, false);
  }

  void pausePreview() {
    if (preview_.isPaused()) {
      preview_.resume();
      if (positionPollTimer_) positionPollTimer_->start();
      if (window_) window_->setPreviewStatus(currentPreviewTitle_, currentPreviewDurationMs_, true, false);
    } else {
      if (preview_.pause()) {
        if (positionPollTimer_) positionPollTimer_->stop();
        if (window_) window_->setPreviewStatus(currentPreviewTitle_, currentPreviewDurationMs_, true, true);
      }
    }
  }

  void seekPreview(int posMs) {
    if (preview_.seekTo(posMs)) {
      if (positionPollTimer_) positionPollTimer_->start();
      if (window_) window_->setPreviewStatus(currentPreviewTitle_, currentPreviewDurationMs_, true, false);
    }
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

  void createBoard(const QString& name, int rows, int cols) {
    const int safeRows = qBound(1, rows, 50);
    const int safeCols = qBound(1, cols, 50);
    BoardRecord board = createBoardRecord(name.trimmed().isEmpty() ? QStringLiteral("New Board") : name.trimmed(), safeCols, safeRows);
    state_.activeBoardId = board.id;
    state_.boards.push_back(board);
    storage_.saveState(state_);
    refreshWindow();
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

  void changeCellEmoji(int cellIndex, const QString& emoji) {
    if (cellIndex < 0) {
      return;
    }

    for (BoardRecord& board : state_.boards) {
      if (board.id == state_.activeBoardId && cellIndex < board.cells.size()) {
        board.cells[cellIndex].icon = emoji;
        board.cells[cellIndex].iconExplicit = true;
        storage_.saveState(state_);
        refreshWindow();
        return;
      }
    }
  }

  void renameSoundDisplay(const QString& soundId, const QString& displayName) {
    for (SoundRecord& sound : state_.library) {
      if (sound.soundId == soundId) {
        sound.displayName = displayName;
        storage_.saveState(state_);
        refreshWindow();
        return;
      }
    }
  }

  void deleteBoard(const QString& boardId) {
    if (state_.boards.size() <= 1) return;
    int idx = -1;
    for (int i = 0; i < state_.boards.size(); ++i) {
      if (state_.boards[i].id == boardId) { idx = i; break; }
    }
    if (idx < 0) return;
    state_.boards.removeAt(idx);
    if (state_.activeBoardId == boardId) {
      state_.activeBoardId = state_.boards.front().id;
    }
    storage_.saveState(state_);
    refreshWindow();
  }

  void renameBoard(const QString& boardId, const QString& newName) {
    for (BoardRecord& board : state_.boards) {
      if (board.id == boardId) {
        board.name = newName;
        storage_.saveState(state_);
        refreshWindow();
        return;
      }
    }
  }

  void deleteSound(const QString& soundId) {
    if (storage_.deleteSound(soundId, state_)) {
      for (BoardRecord& board : state_.boards) {
        for (Cell& cell : board.cells) {
          if (cell.soundId == soundId) cell.soundId.clear();
        }
        board.unassignedSoundIds.removeAll(soundId);
      }
      storage_.saveState(state_);
      refreshWindow();
    }
  }

  void setCellHotkey(int cellIndex, const QString& hotkey) {
    if (cellIndex < 0) {
      return;
    }
    for (BoardRecord& board : state_.boards) {
      if (board.id == state_.activeBoardId && cellIndex < board.cells.size()) {
        board.cells[cellIndex].hotkey = hotkey;
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

  QString downloadYouTube(const YouTubeSearchResult& result, QString* errorMessage,
                          std::atomic<bool>* cancelFlag = nullptr,
                          std::atomic<int>* progressPct = nullptr) {
    QStringList existingNames;
    for (const SoundRecord& sound : state_.library) {
      existingNames.push_back(sound.filename);
    }

    QString filename;
    if (!youtube_.downloadAudio(result, storage_.soundsDir(), existingNames, &filename, errorMessage, cancelFlag, progressPct)) {
      return QString();
    }

    SoundRecord sound = createSoundRecord(filename);
    sound.displayName = result.title;
    sound.sourceType = QStringLiteral("youtube");
    sound.sourceUrl = result.url;
    sound.tags = QStringList{ QStringLiteral("youtube") };
    storage_.refreshSoundMetadata(sound);
    state_.library.push_back(sound);
    storage_.saveState(state_);
    refreshWindow();
    return sound.soundId;
  }

  bool previewYouTube(const YouTubeSearchResult& result, QString* errorMessage) {
    const QString previewRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                  .filePath(QStringLiteral("rpsu_youtube_preview"));
    QDir().mkpath(previewRoot);

    // Stop MCI first so the previous WAV file isn't held open, then delete.
    // Doing it in the wrong order leaks file handles and after many previews
    // the MCI device can get into a bad state.
    preview_.stop();
    if (!lastPreviewPath_.isEmpty()) {
      QFile::remove(lastPreviewPath_);
      lastPreviewPath_.clear();
    }
    // Best-effort cleanup of any stale preview files from earlier sessions.
    cleanupStalePreviewFiles(previewRoot);

    QString previewPath;
    if (!youtube_.downloadPreviewAudio(result, previewRoot, &previewPath, errorMessage)) {
      return false;
    }

    int previewDurationMs = 0;
    QString previewError;
    const QString previewSoundId = QStringLiteral("youtube_preview_%1").arg(result.id);
    if (!preview_.playFile(previewSoundId, previewPath, &previewDurationMs, &previewError)) {
      QFile::remove(previewPath);
      if (errorMessage && errorMessage->isEmpty()) {
        *errorMessage = previewError;
      }
      return false;
    }

    lastPreviewPath_ = previewPath;
    updatePreviewUi(result.title, previewDurationMs, true);
    return true;
  }

  // Remove leftover preview WAVs older than 10 minutes so we don't fill
  // %TEMP%\rpsu_youtube_preview with junk over time.
  void cleanupStalePreviewFiles(const QString& dir) {
    QDir d(dir);
    if (!d.exists()) return;
    const auto cutoff = QDateTime::currentDateTime().addSecs(-600);
    for (const QFileInfo& fi : d.entryInfoList({ QStringLiteral("*.wav") }, QDir::Files)) {
      if (fi.lastModified() < cutoff && fi.absoluteFilePath() != lastPreviewPath_) {
        QFile::remove(fi.absoluteFilePath());
      }
    }
  }

  StorageManager storage_;
  AppState state_;
  PlaybackEngine playbackEngine_;
  PreviewPlayer preview_;
  YouTubeService youtube_;
  MainWindow* window_ = nullptr;
  QTimer* previewClearTimer_ = nullptr;
  QTimer* positionPollTimer_ = nullptr;
  QString lastPreviewPath_;
  QString currentPreviewTitle_;
  int currentPreviewDurationMs_ = 0;
};

}  // namespace rpsu
