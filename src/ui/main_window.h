#pragma once

#include <atomic>
#include <functional>

#include <QWidget>

#include "src/core/models.h"
#include "src/core/youtube_service.h"

class QComboBox;
class QCheckBox;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QTableWidget;
class QFrame;
class QToolButton;

namespace rpsu {

class MainWindow : public QWidget {
 public:
  explicit MainWindow(QWidget* parent = nullptr);

  void setState(const AppState& state);
  int selectedCellIndex() const { return selectedCellIndex_; }
  void setPreviewStatus(const QString& title, int durationMs, bool playing, bool paused = false);
  void updatePreviewProgress(int posMs);
  void showUpdateBanner(const QString& version, const QString& url);

  std::function<void(const QString& boardId)> onBoardSelected;
  std::function<void(const QString& name, int rows, int cols)> onCreateBoard;
  std::function<void(const QString& soundId)> onPlaySound;
  std::function<void()> onStopPreview;
  std::function<void()> onPausePreview;
  std::function<void(int posMs)> onSeekPreview;
  std::function<void(int cellIndex)> onImportSound;
  std::function<void(const QString& soundId, int cellIndex)> onAssignSoundToCell;
  std::function<QVector<YouTubeSearchResult>(const QString& query, int limit, QString* errorMessage)> onYouTubeSearch;
  std::function<bool(const YouTubeSearchResult& result, QString* errorMessage)> onYouTubePreview;
  std::function<QString(const YouTubeSearchResult& result, QString* errorMessage, std::atomic<bool>* cancelFlag, std::atomic<int>* progressPct)> onYouTubeDownload;
  std::function<void(const QString& apiKey)> onFreesoundApiKeyChanged;
  std::function<void(bool darkMode)> onDarkModeChanged;
  std::function<void(int value)> onVolumeRemoteChanged;
  std::function<void(int value)> onVolumeLocalChanged;
  std::function<void(bool enabled)> onPlaybackLocalChanged;
  std::function<void(bool enabled)> onMuteMyselfDuringPlaybackChanged;
  std::function<void(bool enabled)> onShowHotkeysOnButtonsChanged;
  std::function<void(bool enabled)> onGlobalHotkeysEnabledChanged;
  std::function<void(int rows, int cols)> onActiveBoardSizeChanged;
  std::function<void(int cellIndex, const QString& emoji)> onCellEmojiChanged;
  std::function<void(const QString& soundId, const QString& displayName)> onSoundRenamed;
  // Persist new trim values for a sound record.
  std::function<void(const QString& soundId, int trimStartMs, int trimEndMs)> onSoundTrimChanged;
  // One-shot preview with override trim values (does not persist). Used by
  // the Trim dialog so the user can audition cuts before saving.
  std::function<void(const QString& soundId, int trimStartMs, int trimEndMs)> onPreviewSoundWithTrim;
  std::function<void(int cellIndex, const QString& hotkey)> onCellHotkeyChanged;
  std::function<void(const QString& boardId)> onDeleteBoard;
  std::function<void(const QString& boardId, const QString& newName)> onRenameBoard;
  std::function<void(const QString& soundId)> onDeleteSound;
  std::function<void(const QString& sortKey)> onLibrarySortChanged;
  std::function<void(bool enabled)> onLibraryHideAssignedChanged;

  // Fired whenever preview status or position changes — used by the YouTube
  // dialog to mirror the preview bar without polling.
  std::function<void(const QString& title, int durationMs, bool playing, bool paused)> onPreviewStatusChanged;
  std::function<void(int posMs)> onPreviewProgressChanged;

 private:
  void rebuild();
  void applyTheme();
  QString buildCellButtonLabel(const Cell& cell, const QString& fallbackText) const;
  const BoardRecord* activeBoard() const;
  void setSelectedCell(int cellIndex);
  void handleCellClick(int cellIndex, const QString& soundId);
  void openCreateBoardDialog();
  void openYouTubeDialog();
  void showRenameDialog(const QString& soundId);
  void showHelpDialog();
  void showTrimDialog(const QString& soundId);
  QString displayNameForItem(const QListWidgetItem* item) const;

  AppState state_;
  QComboBox* boardSelector_ = nullptr;
  QToolButton* addBoardButton_ = nullptr;
  QToolButton* deleteBoardButton_ = nullptr;
  QToolButton* renameBoardButton_ = nullptr;
  QWidget* boardNavFrame_ = nullptr;
  QWidget* pagerFrame_ = nullptr;
  QScrollArea* gridScrollArea_ = nullptr;
  QWidget* gridHost_ = nullptr;
  QHBoxLayout* boardNavLayout_ = nullptr;
  QHBoxLayout* pagerLayout_ = nullptr;
  QGridLayout* gridLayout_ = nullptr;
  QListWidget* libraryList_ = nullptr;
  QLineEdit* librarySearch_ = nullptr;
  QComboBox* librarySortCombo_ = nullptr;
  QCheckBox* libraryHideAssignedCheckbox_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QPushButton* dismissUpdateButton_ = nullptr;
  QFrame* previewBar_ = nullptr;
  QLabel* previewLabel_ = nullptr;
  QLabel* liveIndicator_ = nullptr;
  QPushButton* pausePreviewButton_ = nullptr;
  QPushButton* resumePreviewButton_ = nullptr;
  QPushButton* stopPreviewButton_ = nullptr;
  QSlider* progressSlider_ = nullptr;
  bool sliderDragging_ = false;
  QLineEdit* freesoundApiKey_ = nullptr;
  QPushButton* youtubeButton_ = nullptr;
  QPushButton* importButton_ = nullptr;
  QSlider* volumeRemoteSlider_ = nullptr;
  QSlider* volumeLocalSlider_ = nullptr;
  QCheckBox* muteOnClientCheckbox_ = nullptr;
  QCheckBox* muteMyselfDuringPlaybackCheckbox_ = nullptr;
  QCheckBox* showHotkeysOnButtonsCheckbox_ = nullptr;
  QCheckBox* disableHotkeysCheckbox_ = nullptr;
  QSpinBox* rowsSpin_ = nullptr;
  QSpinBox* colsSpin_ = nullptr;
  QToolButton* darkModeButton_ = nullptr;
  QToolButton* helpButton_ = nullptr;
  QFrame* settingsFrame_ = nullptr;
  int selectedCellIndex_ = -1;
  bool rebuildingUi_ = false;
  bool darkMode_ = true;
  QVector<QWidget*> cellCards_;
  QVector<QPushButton*> pageButtons_;
};

}  // namespace rpsu
