#include "src/ui/main_window.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace rpsu {

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
  setWindowTitle(QStringLiteral("RP Soundboard Ultimate"));
  resize(980, 640);

  auto* root = new QVBoxLayout(this);
  auto* topBar = new QHBoxLayout();
  auto* importButton = new QPushButton(QStringLiteral("Import Sound"), this);
  boardSelector_ = new QComboBox(this);
  freesoundApiKey_ = new QLineEdit(this);
  freesoundApiKey_->setPlaceholderText(QStringLiteral("Freesound API key"));
  statusLabel_ = new QLabel(QStringLiteral("Plugin mode: direct TS3 injection"), this);

  topBar->addWidget(new QLabel(QStringLiteral("Board"), this));
  topBar->addWidget(boardSelector_, 1);
  topBar->addWidget(importButton);
  topBar->addWidget(freesoundApiKey_, 1);
  root->addLayout(topBar);
  root->addWidget(statusLabel_);

  auto* body = new QHBoxLayout();
  gridHost_ = new QWidget(this);
  gridLayout_ = new QGridLayout(gridHost_);
  libraryList_ = new QListWidget(this);
  libraryList_->setMinimumWidth(300);

  body->addWidget(gridHost_, 2);
  body->addWidget(libraryList_, 1);
  root->addLayout(body, 1);

  connect(boardSelector_, &QComboBox::currentTextChanged, this, [this]() {
    const QString boardId = boardSelector_->currentData().toString();
    if (onBoardSelected) {
      onBoardSelected(boardId);
    }
  });

  connect(importButton, &QPushButton::clicked, this, [this]() {
    if (onImportSound) {
      onImportSound();
    }
  });

  connect(libraryList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
    if (item && onPlaySound) {
      onPlaySound(item->data(Qt::UserRole).toString());
    }
  });

  connect(freesoundApiKey_, &QLineEdit::editingFinished, this, [this]() {
    if (onFreesoundApiKeyChanged) {
      onFreesoundApiKeyChanged(freesoundApiKey_->text().trimmed());
    }
  });
}

void MainWindow::setState(const AppState& state) {
  state_ = state;
  rebuild();
}

const BoardRecord* MainWindow::activeBoard() const {
  for (const BoardRecord& board : state_.boards) {
    if (board.id == state_.activeBoardId) {
      return &board;
    }
  }
  return state_.boards.isEmpty() ? nullptr : &state_.boards.front();
}

void MainWindow::rebuild() {
  boardSelector_->blockSignals(true);
  boardSelector_->clear();
  for (const BoardRecord& board : state_.boards) {
    boardSelector_->addItem(board.name, board.id);
    if (board.id == state_.activeBoardId) {
      boardSelector_->setCurrentIndex(boardSelector_->count() - 1);
    }
  }
  boardSelector_->blockSignals(false);

  freesoundApiKey_->setText(state_.config.freesoundApiKey);

  while (QLayoutItem* item = gridLayout_->takeAt(0)) {
    delete item->widget();
    delete item;
  }

  const BoardRecord* board = activeBoard();
  if (board) {
    for (int index = 0; index < board->cells.size(); ++index) {
      const int row = index / board->cols;
      const int col = index % board->cols;
      QString label = QStringLiteral("+");
      QString soundId;
      if (!board->cells[index].soundId.isEmpty()) {
        soundId = board->cells[index].soundId;
        for (const SoundRecord& sound : state_.library) {
          if (sound.soundId == soundId) {
            label = sound.displayName;
            break;
          }
        }
      }

      auto* button = new QPushButton(label, this);
      button->setMinimumHeight(72);
      gridLayout_->addWidget(button, row, col);
      connect(button, &QPushButton::clicked, this, [this, soundId]() {
        if (!soundId.isEmpty() && onPlaySound) {
          onPlaySound(soundId);
        }
      });
    }
  }

  libraryList_->clear();
  for (const SoundRecord& sound : state_.library) {
    auto* item = new QListWidgetItem(QStringLiteral("%1 [%2]").arg(sound.displayName, sound.sourceType), libraryList_);
    item->setData(Qt::UserRole, sound.soundId);
  }
}

}  // namespace rpsu
