#pragma once

#include <atomic>

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
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
#include "src/core/update_checker.h"
#include "src/engine/playback_engine.h"
#include "src/core/youtube_service.h"
#include "src/ui/main_window.h"
#include "src/version.h"

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
      // Cache the title/duration so pausePreview() can pass them back to the
      // UI (setPreviewStatus disables buttons if title is empty, which used to
      // make pause look like stop in TS3-routing mode).
      if (playing) {
        currentPreviewTitle_ = title;
        currentPreviewDurationMs_ = durationMs;
        if (positionPollTimer_) positionPollTimer_->start();
      } else {
        if (positionPollTimer_) positionPollTimer_->stop();
        currentlyPlayingSoundId_.clear();
      }
      updatePreviewUi(title, durationMs, playing);
    };
    playbackEngine_.initialize();
    applyRuntimeConfig();
    storage_.saveState(state_);

    QObject::connect(&updateChecker_, &UpdateChecker::updateAvailable,
      [this](const QString& latestVersion, const QString& releaseUrl) {
        pendingUpdateVersion_ = latestVersion;
        pendingUpdateUrl_ = releaseUrl;
        if (window_) {
          window_->showUpdateBanner(pendingUpdateVersion_, pendingUpdateUrl_);
        }
      });
    updateChecker_.checkAsync(QString::fromLatin1(RPSU_VERSION_STRING));

    return true;
  }

  void shutdown() {
    // Order matters: stop anything that could fire callbacks into the window
    // (timers, network replies) BEFORE we tear the window down, otherwise the
    // callback lambdas reference a deleted QObject and crash TS3 on
    // deactivate.
    //
    // The timers themselves are kept alive (only stopped) because they are
    // owned by the PluginContext singleton and need to survive across
    // shutdown/init cycles — TS3 can re-activate the plugin without
    // destroying our singleton.

    if (positionPollTimer_) {
      positionPollTimer_->stop();
    }
    if (previewClearTimer_) {
      previewClearTimer_->stop();
    }

    // Drop the updateChecker signal connection so a late HTTP reply doesn't
    // call window_->showUpdateBanner() on a destroyed widget. The connection
    // is re-established in initialize().
    updateChecker_.disconnect();

    preview_.stop();
    playbackEngine_.shutdown();

    if (window_) {
      // Disconnect all signals so any queued callbacks targeting the window
      // (e.g. QTimer::singleShot lambdas posted to it) drop instead of
      // dereferencing a deleted widget.
      window_->disconnect();
      window_->hide();
      window_->deleteLater();
      window_ = nullptr;

      // Drain the deferred-delete queue before returning to the loader. The
      // loader does NOT FreeLibrary us, but the next ts3plugin_init() should
      // start from a clean slate.
      if (QCoreApplication::instance()) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      }
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
      window_->onDarkModeChanged = [this](bool darkMode) {
        state_.config.darkMode = darkMode;
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
      window_->onSoundTrimChanged = [this](const QString& soundId, int startMs, int endMs) {
        updateSoundTrim(soundId, startMs, endMs);
      };
      window_->onPreviewSoundWithTrim = [this](const QString& soundId, int startMs, int endMs) {
        previewSoundWithTrim(soundId, startMs, endMs);
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
      window_->onLibrarySortChanged = [this](const QString& sortKey) {
        state_.config.librarySortKey = sortKey;
        storage_.saveState(state_);
      };
      window_->onLibraryHideAssignedChanged = [this](bool enabled) {
        state_.config.libraryHideAssigned = enabled;
        storage_.saveState(state_);
      };
    }

    refreshWindow();
    window_->show();
    window_->raise();
    window_->activateWindow();

    if (!pendingUpdateVersion_.isEmpty()) {
      window_->showUpdateBanner(pendingUpdateVersion_, pendingUpdateUrl_);
    }
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
        currentlyPlayingSoundId_ = soundId;
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
      if (!window_) return;
      if (playbackEngine_.isActive()) {
        const int pos = playbackEngine_.getPositionMs();
        if (pos >= 0) window_->updatePreviewProgress(pos);
      } else if (preview_.isActive()) {
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
    // The button serves both audio paths: the MCI preview (YouTube previews,
    // non-TS3 mode) and the Sampler-routed playback (TS3 mode). Pick whichever
    // is currently active so a single button works in both contexts.
    if (playbackEngine_.isActive()) {
      if (playbackEngine_.isPaused()) {
        playbackEngine_.resumePlayback();
        if (window_) window_->setPreviewStatus(currentPreviewTitle_, currentPreviewDurationMs_, true, false);
      } else {
        playbackEngine_.pausePlayback();
        if (window_) window_->setPreviewStatus(currentPreviewTitle_, currentPreviewDurationMs_, true, true);
      }
      return;
    }
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
    if (playbackEngine_.isActive()) {
      if (playbackEngine_.seekTo(posMs)) {
        if (positionPollTimer_) positionPollTimer_->start();
        if (window_) window_->setPreviewStatus(currentPreviewTitle_, currentPreviewDurationMs_, true, false);
      }
    } else if (preview_.seekTo(posMs)) {
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
    const int nextRows = qBound(1, rows, kMaxBoardDimension);
    const int nextCols = qBound(1, cols, kMaxBoardDimension);
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
    const int safeRows = qBound(1, rows, kMaxBoardDimension);
    const int safeCols = qBound(1, cols, kMaxBoardDimension);
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
        // If the renamed sound is currently in the preview bar (playing or
        // paused), refresh the title live — the Sampler-side callback only
        // fires on start/stop so it would otherwise keep the old name until
        // playback restarts.
        if (soundId == currentlyPlayingSoundId_) {
          playbackEngine_.setActiveDisplayName(displayName);
          currentPreviewTitle_ = displayName;
          if (window_) {
            window_->setPreviewStatus(
              displayName,
              currentPreviewDurationMs_,
              playbackEngine_.isActive(),
              playbackEngine_.isPaused());
          }
        }
        refreshWindow();
        return;
      }
    }
  }

  void updateSoundTrim(const QString& soundId, int trimStartMs, int trimEndMs) {
    for (SoundRecord& sound : state_.library) {
      if (sound.soundId != soundId) continue;
      sound.trimStartMs = qMax(0, trimStartMs);
      sound.trimEndMs   = qMax(0, trimEndMs);
      storage_.saveState(state_);
      // Stop any in-flight playback of this sound so the next play decodes
      // a fresh PCM segment with the new trim instead of finishing the
      // stale window from the previous trim values.
      if (soundId == currentlyPlayingSoundId_) {
        playbackEngine_.stopPlayback();
        preview_.stop();
        currentlyPlayingSoundId_.clear();
      }
      refreshWindow();
      return;
    }
  }

  // Play one-shot with override trim values without persisting them, so the
  // Trim dialog can audition cuts before the user commits with Save.
  void previewSoundWithTrim(const QString& soundId, int trimStartMs, int trimEndMs) {
    for (const SoundRecord& sound : state_.library) {
      if (sound.soundId != soundId) continue;

      SoundRecord override = sound;
      override.trimStartMs = qMax(0, trimStartMs);
      override.trimEndMs   = qMax(0, trimEndMs);

      QString error;
      bool started = false;
#ifdef RPSU_ENABLE_TS3_ROUTING
      started = playbackEngine_.playSound(override, storage_.soundsDir(), &error);
#else
      const QString filePath = QDir(storage_.soundsDir()).filePath(sound.filename);
      int previewDurationMs = 0;
      QString previewError;
      started = preview_.playFile(sound.soundId, filePath, &previewDurationMs, &previewError);
      if (!started) error = previewError;
      if (started) {
        currentPreviewTitle_ = sound.displayName;
        currentPreviewDurationMs_ = previewDurationMs;
        if (positionPollTimer_) positionPollTimer_->start();
        updatePreviewUi(sound.displayName, previewDurationMs, true);
      }
#endif
      if (started) {
        currentlyPlayingSoundId_ = soundId;
      }
      if (!started && !error.isEmpty() && window_) {
        window_->setPreviewStatus(QStringLiteral("Preview failed: %1").arg(error), 0, false);
      }
      return;
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
    // All state_ writes and Qt widget calls must happen on the main thread.
    // Capture sound by value so the bg thread doesn't race with state_.library.
    QTimer::singleShot(0, window_, [this, sound]() {
      state_.library.push_back(sound);
      storage_.saveState(state_);
      refreshWindow();
    });
    return sound.soundId;
  }

  bool previewYouTube(const YouTubeSearchResult& result, QString* errorMessage) {
    const QString previewRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                  .filePath(QStringLiteral("rpsu_youtube_preview"));
    QDir().mkpath(previewRoot);
    cleanupStalePreviewFiles(previewRoot);

    QString previewPath;
    if (!youtube_.downloadPreviewAudio(result, previewRoot, &previewPath, errorMessage)) {
      return false;
    }

    // MCI must be driven from the main (UI) thread — its alias lifetime and
    // internal window messages are tied to the thread that opened it. Calling
    // playFile/stop from the async future races with the dialog/main loop and
    // can leave aliases in a state where stop() silently fails. Post all MCI
    // work and UI updates to the main thread.
    const QString title = result.title;
    const QString previewSoundId = QStringLiteral("youtube_preview_%1").arg(result.id);
    QTimer::singleShot(0, window_, [this, previewPath, previewSoundId, title]() {
      preview_.stop();
      if (!lastPreviewPath_.isEmpty()) {
        QFile::remove(lastPreviewPath_);
        lastPreviewPath_.clear();
      }

      int previewDurationMs = 0;
      QString previewError;
      if (!preview_.playFile(previewSoundId, previewPath, &previewDurationMs, &previewError)) {
        QFile::remove(previewPath);
        if (window_) {
          window_->setPreviewStatus(
            previewError.isEmpty() ? QStringLiteral("Preview failed.") : previewError,
            0, false);
        }
        return;
      }

      lastPreviewPath_ = previewPath;
      currentPreviewTitle_ = title;
      currentPreviewDurationMs_ = previewDurationMs;
      if (positionPollTimer_) positionPollTimer_->start();
      updatePreviewUi(title, previewDurationMs, true);
    });
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
  UpdateChecker updateChecker_;
  MainWindow* window_ = nullptr;
  QTimer* previewClearTimer_ = nullptr;
  QTimer* positionPollTimer_ = nullptr;
  QString lastPreviewPath_;
  QString currentPreviewTitle_;
  // Soundboard sound id of the audio currently in the preview bar (playing
  // or paused). Used to keep the title/trim in sync when the user renames
  // or re-trims the sound while it is active.
  QString currentlyPlayingSoundId_;
  int currentPreviewDurationMs_ = 0;
  QString pendingUpdateVersion_;
  QString pendingUpdateUrl_;
};

}  // namespace rpsu
