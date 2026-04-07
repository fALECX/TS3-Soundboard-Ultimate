#pragma once

#include <functional>

#include <QWidget>

#include "src/core/models.h"
#include "src/core/youtube_service.h"

class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
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
  std::function<void(const QString& apiKey)> onFreesoundApiKeyChanged;

 private:
  void rebuild();
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
  QPushButton* stopPreviewButton_ = nullptr;
  QLineEdit* freesoundApiKey_ = nullptr;
  QPushButton* youtubeButton_ = nullptr;
  int selectedCellIndex_ = -1;
  QVector<QPushButton*> cellButtons_;
};

}  // namespace rpsu
