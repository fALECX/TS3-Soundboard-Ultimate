#pragma once

#include <functional>

#include <QWidget>

#include "src/core/models.h"
#include "src/core/youtube_service.h"

class QComboBox;
class QCheckBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableWidget;
class QFrame;

namespace rpsu {

class MainWindow : public QWidget {
 public:
  explicit MainWindow(QWidget* parent = nullptr);

  void setState(const AppState& state);
  int selectedCellIndex() const { return selectedCellIndex_; }
  void setPreviewStatus(const QString& title, int durationMs, bool playing);

  std::function<void(const QString& boardId)> onBoardSelected;
  std::function<void(const QString& soundId)> onPlaySound;
  std::function<void()> onStopPreview;
  std::function<void(int cellIndex)> onImportSound;
  std::function<void(const QString& soundId, int cellIndex)> onAssignSoundToCell;
  std::function<QVector<YouTubeSearchResult>(const QString& query, int limit, QString* errorMessage)> onYouTubeSearch;
  std::function<QString(const YouTubeSearchResult& result, QString* errorMessage)> onYouTubeDownload;
  std::function<bool(const YouTubeSearchResult& result, QString* errorMessage)> onYouTubePreview;
  std::function<void(const QString& apiKey)> onFreesoundApiKeyChanged;
  std::function<void(int value)> onVolumeRemoteChanged;
  std::function<void(int value)> onVolumeLocalChanged;
  std::function<void(bool enabled)> onPlaybackLocalChanged;
  std::function<void(bool enabled)> onMuteMyselfDuringPlaybackChanged;
  std::function<void(bool enabled)> onShowHotkeysOnButtonsChanged;
  std::function<void(bool enabled)> onGlobalHotkeysEnabledChanged;
  std::function<void(int rows, int cols)> onActiveBoardSizeChanged;

 private:
  void rebuild();
  QString buildCellButtonLabel(const Cell& cell, const QString& fallbackText) const;
  const BoardRecord* activeBoard() const;
  void setSelectedCell(int cellIndex);
  void handleCellClick(int cellIndex, const QString& soundId);
  void openYouTubeDialog();

  AppState state_;
  QComboBox* boardSelector_ = nullptr;
  QWidget* gridHost_ = nullptr;
  QGridLayout* gridLayout_ = nullptr;
  QListWidget* libraryList_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QFrame* previewBar_ = nullptr;
  QLabel* previewLabel_ = nullptr;
  QLabel* liveIndicator_ = nullptr;
  QPushButton* stopPreviewButton_ = nullptr;
  QLineEdit* freesoundApiKey_ = nullptr;
  QPushButton* youtubeButton_ = nullptr;
  QSlider* volumeRemoteSlider_ = nullptr;
  QSlider* volumeLocalSlider_ = nullptr;
  QCheckBox* muteOnClientCheckbox_ = nullptr;
  QCheckBox* muteMyselfDuringPlaybackCheckbox_ = nullptr;
  QCheckBox* showHotkeysOnButtonsCheckbox_ = nullptr;
  QCheckBox* disableHotkeysCheckbox_ = nullptr;
  QSpinBox* rowsSpin_ = nullptr;
  QSpinBox* colsSpin_ = nullptr;
  int selectedCellIndex_ = -1;
  bool rebuildingUi_ = false;
  QVector<QPushButton*> cellButtons_;
  QVector<QPushButton*> deleteButtons_;
};

}  // namespace rpsu
