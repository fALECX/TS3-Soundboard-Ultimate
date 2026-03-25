#pragma once

#include <functional>

#include <QWidget>

#include "src/core/models.h"

class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;

namespace rpsu {

class MainWindow : public QWidget {
 public:
  explicit MainWindow(QWidget* parent = nullptr);

  void setState(const AppState& state);

  std::function<void(const QString& boardId)> onBoardSelected;
  std::function<void(const QString& soundId)> onPlaySound;
  std::function<void()> onImportSound;
  std::function<void(const QString& apiKey)> onFreesoundApiKeyChanged;

 private:
  void rebuild();
  const BoardRecord* activeBoard() const;

  AppState state_;
  QComboBox* boardSelector_ = nullptr;
  QWidget* gridHost_ = nullptr;
  QGridLayout* gridLayout_ = nullptr;
  QListWidget* libraryList_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QLineEdit* freesoundApiKey_ = nullptr;
};

}  // namespace rpsu
