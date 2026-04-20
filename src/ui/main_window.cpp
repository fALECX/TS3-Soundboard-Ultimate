#include "src/ui/main_window.h"

#include <chrono>
#include <future>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDesktopServices>
#include <QEventLoop>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSize>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace rpsu {

namespace {

QString formatDuration(int seconds) {
  if (seconds <= 0) {
    return QStringLiteral("--:--");
  }

  const int minutes = seconds / 60;
  const int remaining = seconds % 60;
  return QStringLiteral("%1:%2").arg(minutes).arg(remaining, 2, 10, QLatin1Char('0'));
}

QString formatDurationMs(int durationMs) {
  return formatDuration(qMax(0, durationMs / 1000));
}

class YouTubeSearchDialog : public QDialog {
 public:
  explicit YouTubeSearchDialog(MainWindow* owner) : QDialog(owner), owner_(owner) {
    setWindowTitle(QStringLiteral("YouTube Search"));
    resize(980, 620);
    setStyleSheet(
      QStringLiteral(
        "QDialog { background: #f5f7fb; }"
        "QLineEdit { padding: 8px 10px; border: 1px solid #cfd8e3; border-radius: 8px; background: white; }"
        "QPushButton { padding: 8px 12px; border: 1px solid #c6d2e1; border-radius: 8px; background: white; }"
        "QPushButton:hover { background: #eef4fb; }"
        "QPushButton:disabled { color: #8c97a8; background: #eef1f5; }"
        "QTableWidget { background: white; border: 1px solid #d7dfeb; border-radius: 10px; gridline-color: #ecf1f7; }"
        "QHeaderView::section { background: #eef3f9; padding: 8px; border: none; border-bottom: 1px solid #d7dfeb; color: #2b3442; font-weight: 600; }"
      )
    );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);

    auto* titleLabel = new QLabel(QStringLiteral("YouTube Search"), this);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 700; color: #1f2937;"));
    root->addWidget(titleLabel);

    auto* topRow = new QHBoxLayout();
    queryEdit_ = new QLineEdit(this);
    queryEdit_->setPlaceholderText(QStringLiteral("Search YouTube"));
    searchButton_ = new QPushButton(QStringLiteral("Search"), this);
    loadMoreButton_ = new QPushButton(QStringLiteral("Load More"), this);
    loadMoreButton_->setEnabled(false);
    topRow->addWidget(queryEdit_, 1);
    topRow->addWidget(searchButton_);
    topRow->addWidget(loadMoreButton_);
    root->addLayout(topRow);

    statusLabel_ = new QLabel(QStringLiteral("Enter a search term and press Search."), this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #556274;"));
    root->addWidget(statusLabel_);

    resultsTable_ = new QTableWidget(this);
    resultsTable_->setColumnCount(4);
    resultsTable_->setHorizontalHeaderLabels({ QStringLiteral("Title"), QStringLiteral("Length"), QStringLiteral("Channel"), QStringLiteral("Actions") });
    resultsTable_->verticalHeader()->setVisible(false);
    resultsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsTable_->setAlternatingRowColors(true);
    resultsTable_->setShowGrid(false);
    resultsTable_->horizontalHeader()->setStretchLastSection(false);
    resultsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resultsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    resultsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    resultsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    resultsTable_->setColumnWidth(1, 90);
    resultsTable_->setColumnWidth(2, 220);
    resultsTable_->setColumnWidth(3, 210);
    resultsTable_->setStyleSheet(
      QStringLiteral(
        "QTableWidget::item { padding: 10px 8px; border-bottom: 1px solid #edf2f8; }"
        "QTableWidget::item:selected { background: #dcecff; color: #162033; }"
        "QTableWidget { alternate-background-color: #f9fbfe; }"
      )
    );
    root->addWidget(resultsTable_, 1);

    auto* buttons = new QHBoxLayout();
    downloadButton_ = new QPushButton(QStringLiteral("Download Selected"), this);
    closeButton_ = new QPushButton(QStringLiteral("Close"), this);
    buttons->addStretch(1);
    buttons->addWidget(downloadButton_);
    buttons->addWidget(closeButton_);
    root->addLayout(buttons);

    connect(searchButton_, &QPushButton::clicked, this, [this]() { runSearch(); });
    connect(loadMoreButton_, &QPushButton::clicked, this, [this]() { loadMore(); });
    connect(queryEdit_, &QLineEdit::returnPressed, this, [this]() { runSearch(); });
    connect(downloadButton_, &QPushButton::clicked, this, [this]() { downloadSelected(); });
    connect(closeButton_, &QPushButton::clicked, this, &QDialog::reject);
    connect(resultsTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
      resultsTable_->selectRow(row);
      downloadSelected();
    });
  }

 private:
  template <typename Fn>
  auto runWithLoading(const QString& baseMessage, Fn&& fn) -> decltype(fn()) {
    setBusy(true);
    loadingTick_ = 0;

    auto future = std::async(std::launch::async, std::forward<Fn>(fn));
    while (future.wait_for(std::chrono::milliseconds(80)) != std::future_status::ready) {
      updateLoadingMessage(baseMessage);
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    updateLoadingMessage(baseMessage);
    auto result = future.get();
    setBusy(false);
    return result;
  }

  void setBusy(bool busy) {
    busy_ = busy;
    queryEdit_->setEnabled(!busy);
    searchButton_->setEnabled(!busy);
    loadMoreButton_->setEnabled(!busy && !lastQuery_.isEmpty());
    downloadButton_->setEnabled(!busy);
    closeButton_->setEnabled(!busy);
    resultsTable_->setEnabled(!busy);
    setCursor(busy ? Qt::BusyCursor : Qt::ArrowCursor);
  }

  void updateLoadingMessage(const QString& baseMessage) {
    static const char* frames[] = { ".", "..", "...", "...." };
    statusLabel_->setText(baseMessage + QString::fromLatin1(frames[loadingTick_ % 4]));
    ++loadingTick_;
  }

  void runSearch() {
    const QString query = queryEdit_->text().trimmed();
    if (query.isEmpty()) {
      statusLabel_->setText(QStringLiteral("Enter a search term first."));
      return;
    }

    lastQuery_ = query;
    currentLimit_ = kPageSize;
    refreshResults(true);
  }

  void loadMore() {
    if (lastQuery_.isEmpty()) {
      return;
    }
    currentLimit_ += kPageSize;
    refreshResults(false);
  }

  void refreshResults(bool resetSelection) {
    results_.clear();
    resultsTable_->setRowCount(0);

    if (!owner_->onYouTubeSearch) {
      statusLabel_->setText(QStringLiteral("YouTube search is not available in this build."));
      return;
    }

    QString error;
    results_ = runWithLoading(QStringLiteral("Searching YouTube"), [this, &error]() {
      return owner_->onYouTubeSearch(lastQuery_, currentLimit_, &error);
    });
    if (results_.isEmpty()) {
      statusLabel_->setText(error.isEmpty() ? QStringLiteral("No YouTube results found.") : error);
      loadMoreButton_->setEnabled(false);
      return;
    }

    for (int index = 0; index < results_.size(); ++index) {
      const YouTubeSearchResult& result = results_[index];
      resultsTable_->insertRow(index);

      auto* titleItem = new QTableWidgetItem(result.title);
      titleItem->setToolTip(result.title);
      resultsTable_->setItem(index, 0, titleItem);
      resultsTable_->setItem(index, 1, new QTableWidgetItem(formatDuration(result.durationSeconds)));
      resultsTable_->setItem(index, 2, new QTableWidgetItem(result.channel));

      auto* actionHost = new QWidget(resultsTable_);
      auto* actionLayout = new QHBoxLayout(actionHost);
      actionLayout->setContentsMargins(6, 4, 6, 4);
      actionLayout->setSpacing(6);

      auto* previewButton = new QPushButton(QStringLiteral("Preview"), actionHost);
      auto* downloadRowButton = new QPushButton(QStringLiteral("Download"), actionHost);
      previewButton->setCursor(Qt::PointingHandCursor);
      downloadRowButton->setCursor(Qt::PointingHandCursor);
      previewButton->setStyleSheet(QStringLiteral("QPushButton { padding: 5px 10px; }"));
      downloadRowButton->setStyleSheet(QStringLiteral("QPushButton { padding: 5px 10px; background: #dcecff; border-color: #b7cdee; }"));

      connect(previewButton, &QPushButton::clicked, this, [result]() {
        QDesktopServices::openUrl(QUrl(result.url));
      });
      connect(downloadRowButton, &QPushButton::clicked, this, [this, index]() {
        resultsTable_->selectRow(index);
        downloadSelected();
      });

      actionLayout->addWidget(previewButton);
      actionLayout->addWidget(downloadRowButton);
      actionLayout->addStretch(1);
      resultsTable_->setCellWidget(index, 3, actionHost);
      resultsTable_->setRowHeight(index, 46);
    }

    if (resetSelection && resultsTable_->rowCount() > 0) {
      resultsTable_->selectRow(0);
    }

    const bool canLoadMore = results_.size() >= currentLimit_;
    loadMoreButton_->setEnabled(canLoadMore);
    loadMoreButton_->setText(canLoadMore ? QStringLiteral("Load More") : QStringLiteral("All Loaded"));
    statusLabel_->setText(QStringLiteral("%1 YouTube results loaded.").arg(results_.size()));
  }

  void downloadSelected() {
    const int index = resultsTable_->currentRow();
    if (index < 0 || index >= results_.size()) {
      statusLabel_->setText(QStringLiteral("Select a YouTube result first."));
      return;
    }

    if (!owner_->onYouTubeDownload) {
      statusLabel_->setText(QStringLiteral("YouTube download is not available in this build."));
      return;
    }

    QString error;
    const QString soundId = runWithLoading(QStringLiteral("Downloading audio"), [this, index, &error]() {
      return owner_->onYouTubeDownload(results_[index], &error);
    });
    if (soundId.isEmpty()) {
      statusLabel_->setText(error.isEmpty() ? QStringLiteral("Download failed.") : error);
      return;
    }

    if (owner_->selectedCellIndex() >= 0 && owner_->onAssignSoundToCell) {
      owner_->onAssignSoundToCell(soundId, owner_->selectedCellIndex());
    }
    accept();
  }

  MainWindow* owner_;
  QLineEdit* queryEdit_ = nullptr;
  QLabel* statusLabel_ = nullptr;
  QTableWidget* resultsTable_ = nullptr;
  QVector<YouTubeSearchResult> results_;
  QPushButton* searchButton_ = nullptr;
  QPushButton* loadMoreButton_ = nullptr;
  QPushButton* downloadButton_ = nullptr;
  QPushButton* closeButton_ = nullptr;
  bool busy_ = false;
  int loadingTick_ = 0;
  QString lastQuery_;
  int currentLimit_ = 0;
  static constexpr int kPageSize = 12;
};

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
  setWindowTitle(QStringLiteral("RP Soundboard Ultimate"));
  resize(980, 640);

  auto* root = new QVBoxLayout(this);
  auto* topBar = new QHBoxLayout();
  auto* importButton = new QPushButton(QStringLiteral("Import Sound"), this);
  youtubeButton_ = new QPushButton(QStringLiteral("YouTube Search"), this);
  auto* twitchButton = new QToolButton(this);
  boardSelector_ = new QComboBox(this);
  freesoundApiKey_ = new QLineEdit(this);
  freesoundApiKey_->setPlaceholderText(QStringLiteral("Freesound API key"));
  statusLabel_ = new QLabel(QStringLiteral("Select a cell, then import, search YouTube, or assign from the library."), this);

  // Setup Twitch button
  twitchButton->setToolTip(QStringLiteral("Visit fALECX on Twitch"));
  twitchButton->setIconSize(QSize(32, 32));
  twitchButton->setStyleSheet(
    QStringLiteral(
      "QToolButton { border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 8px; padding: 4px; background: rgba(145, 71, 255, 0.1); }"
      "QToolButton:hover { background: rgba(145, 71, 255, 0.2); border-color: rgba(145, 71, 255, 0.5); }"
    )
  );
  twitchButton->setText(QStringLiteral("Twitch"));
  twitchButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  topBar->addWidget(twitchButton);
  topBar->addWidget(new QLabel(QStringLiteral("Board"), this));
  topBar->addWidget(boardSelector_, 1);
  topBar->addWidget(importButton);
  topBar->addWidget(youtubeButton_);
  topBar->addWidget(freesoundApiKey_, 1);
  root->addLayout(topBar);
  root->addWidget(statusLabel_);

  auto* settingsFrame = new QFrame(this);
  settingsFrame->setFrameShape(QFrame::StyledPanel);
  auto* settingsLayout = new QVBoxLayout(settingsFrame);
  settingsLayout->setContentsMargins(10, 8, 10, 8);
  settingsLayout->setSpacing(8);

  auto* settingsTitle = new QLabel(QStringLiteral("Settings"), settingsFrame);
  settingsTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
  settingsLayout->addWidget(settingsTitle);

  auto* settingsRows = new QHBoxLayout();
  auto* volumeLayout = new QVBoxLayout();
  auto* volumeRemoteRow = new QHBoxLayout();
  volumeRemoteRow->addWidget(new QLabel(QStringLiteral("Volume Remote"), settingsFrame));
  volumeRemoteSlider_ = new QSlider(Qt::Horizontal, settingsFrame);
  volumeRemoteSlider_->setRange(0, 100);
  volumeRemoteSlider_->setPageStep(10);
  volumeRemoteRow->addWidget(volumeRemoteSlider_, 1);
  volumeLayout->addLayout(volumeRemoteRow);

  auto* volumeLocalRow = new QHBoxLayout();
  volumeLocalRow->addWidget(new QLabel(QStringLiteral("Volume Local"), settingsFrame));
  volumeLocalSlider_ = new QSlider(Qt::Horizontal, settingsFrame);
  volumeLocalSlider_->setRange(0, 100);
  volumeLocalSlider_->setPageStep(10);
  volumeLocalRow->addWidget(volumeLocalSlider_, 1);
  volumeLayout->addLayout(volumeLocalRow);
  settingsRows->addLayout(volumeLayout, 3);

  auto* togglesLayout = new QVBoxLayout();
  muteOnClientCheckbox_ = new QCheckBox(QStringLiteral("Mute on my client"), settingsFrame);
  muteMyselfDuringPlaybackCheckbox_ = new QCheckBox(QStringLiteral("Mute myself during playback"), settingsFrame);
  showHotkeysOnButtonsCheckbox_ = new QCheckBox(QStringLiteral("Show hotkeys on buttons"), settingsFrame);
  disableHotkeysCheckbox_ = new QCheckBox(QStringLiteral("Disable hotkeys"), settingsFrame);
  togglesLayout->addWidget(muteOnClientCheckbox_);
  togglesLayout->addWidget(muteMyselfDuringPlaybackCheckbox_);
  togglesLayout->addWidget(showHotkeysOnButtonsCheckbox_);
  togglesLayout->addWidget(disableHotkeysCheckbox_);
  togglesLayout->addStretch(1);
  settingsRows->addLayout(togglesLayout, 2);

  auto* sizeLayout = new QHBoxLayout();
  sizeLayout->addWidget(new QLabel(QStringLiteral("Rows:"), settingsFrame));
  rowsSpin_ = new QSpinBox(settingsFrame);
  rowsSpin_->setRange(1, 50);
  sizeLayout->addWidget(rowsSpin_);
  sizeLayout->addSpacing(8);
  sizeLayout->addWidget(new QLabel(QStringLiteral("Columns:"), settingsFrame));
  colsSpin_ = new QSpinBox(settingsFrame);
  colsSpin_->setRange(1, 50);
  sizeLayout->addWidget(colsSpin_);
  sizeLayout->addStretch(1);

  settingsLayout->addLayout(settingsRows);
  settingsLayout->addLayout(sizeLayout);
  root->addWidget(settingsFrame);

  previewBar_ = new QFrame(this);
  previewBar_->setStyleSheet(QStringLiteral("QFrame { background: #eef4fb; border: 1px solid #d3dfed; border-radius: 8px; }"));
  auto* previewLayout = new QHBoxLayout(previewBar_);
  previewLayout->setContentsMargins(12, 8, 12, 8);
  liveIndicator_ = new QLabel(previewBar_);
  liveIndicator_->setFixedSize(12, 12);
  liveIndicator_->setStyleSheet(QStringLiteral("background-color: #ef4444; border-radius: 6px;"));
  liveIndicator_->setVisible(false);
  previewLabel_ = new QLabel(QStringLiteral("Preview stopped"), previewBar_);
  stopPreviewButton_ = new QPushButton(QStringLiteral("Stop Preview"), previewBar_);
  stopPreviewButton_->setEnabled(false);
  previewLayout->addWidget(liveIndicator_);
  previewLayout->addSpacing(8);
  previewLayout->addWidget(previewLabel_, 1);
  previewLayout->addWidget(stopPreviewButton_);
  root->addWidget(previewBar_);

  auto* body = new QHBoxLayout();
  gridHost_ = new QWidget(this);
  gridLayout_ = new QGridLayout(gridHost_);
  libraryList_ = new QListWidget(this);
  libraryList_->setMinimumWidth(300);

  body->addWidget(gridHost_, 2);
  body->addWidget(libraryList_, 1);
  root->addLayout(body, 1);

  connect(twitchButton, &QToolButton::clicked, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://twitch.tv/fALECX")));
  });

  connect(boardSelector_, &QComboBox::currentTextChanged, this, [this]() {
    const QString boardId = boardSelector_->currentData().toString();
    if (onBoardSelected) {
      onBoardSelected(boardId);
    }
  });

  connect(importButton, &QPushButton::clicked, this, [this]() {
    if (onImportSound) {
      onImportSound(selectedCellIndex_);
    }
  });

  connect(youtubeButton_, &QPushButton::clicked, this, [this]() {
    openYouTubeDialog();
  });

  connect(libraryList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    if (!item) {
      return;
    }
    if (selectedCellIndex_ >= 0) {
      statusLabel_->setText(QStringLiteral("Ready to assign \"%1\" to the selected cell. Double-click to confirm.").arg(item->text()));
    }
  });

  connect(libraryList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
    if (!item) {
      return;
    }
    const QString soundId = item->data(Qt::UserRole).toString();
    const QString itemText = item->text();
    if (selectedCellIndex_ >= 0 && onAssignSoundToCell) {
      onAssignSoundToCell(soundId, selectedCellIndex_);
      statusLabel_->setText(QStringLiteral("Assigned \"%1\" to the selected cell.").arg(itemText));
      return;
    }
    if (onPlaySound) {
      onPlaySound(soundId);
    }
  });

  connect(freesoundApiKey_, &QLineEdit::editingFinished, this, [this]() {
    if (onFreesoundApiKeyChanged) {
      onFreesoundApiKeyChanged(freesoundApiKey_->text().trimmed());
    }
  });

  connect(volumeRemoteSlider_, &QSlider::valueChanged, this, [this](int value) {
    if (rebuildingUi_) {
      return;
    }
    if (onVolumeRemoteChanged) {
      onVolumeRemoteChanged(value);
    }
  });

  connect(volumeLocalSlider_, &QSlider::valueChanged, this, [this](int value) {
    if (rebuildingUi_) {
      return;
    }
    if (onVolumeLocalChanged) {
      onVolumeLocalChanged(value);
    }
  });

  connect(muteOnClientCheckbox_, &QCheckBox::toggled, this, [this](bool checked) {
    if (rebuildingUi_) {
      return;
    }
    if (onPlaybackLocalChanged) {
      onPlaybackLocalChanged(!checked);
    }
  });

  connect(muteMyselfDuringPlaybackCheckbox_, &QCheckBox::toggled, this, [this](bool checked) {
    if (rebuildingUi_) {
      return;
    }
    if (onMuteMyselfDuringPlaybackChanged) {
      onMuteMyselfDuringPlaybackChanged(checked);
    }
  });

  connect(showHotkeysOnButtonsCheckbox_, &QCheckBox::toggled, this, [this](bool checked) {
    if (rebuildingUi_) {
      return;
    }
    if (onShowHotkeysOnButtonsChanged) {
      onShowHotkeysOnButtonsChanged(checked);
    }
  });

  connect(disableHotkeysCheckbox_, &QCheckBox::toggled, this, [this](bool checked) {
    if (rebuildingUi_) {
      return;
    }
    if (onGlobalHotkeysEnabledChanged) {
      onGlobalHotkeysEnabledChanged(!checked);
    }
  });

  connect(rowsSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int rows) {
    if (rebuildingUi_) {
      return;
    }
    if (onActiveBoardSizeChanged) {
      onActiveBoardSizeChanged(rows, colsSpin_->value());
    }
  });

  connect(colsSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int cols) {
    if (rebuildingUi_) {
      return;
    }
    if (onActiveBoardSizeChanged) {
      onActiveBoardSizeChanged(rowsSpin_->value(), cols);
    }
  });

  connect(stopPreviewButton_, &QPushButton::clicked, this, [this]() {
    if (onStopPreview) {
      onStopPreview();
    }
  });
}

void MainWindow::setState(const AppState& state) {
  state_ = state;
  rebuild();
}

QString MainWindow::buildCellButtonLabel(const Cell& cell, const QString& fallbackText) const {
  QString label = fallbackText;
  if (state_.config.showHotkeysOnButtons && !cell.hotkey.trimmed().isEmpty()) {
    label += QStringLiteral("\n[%1]").arg(cell.hotkey);
  }
  return label;
}

const BoardRecord* MainWindow::activeBoard() const {
  for (const BoardRecord& board : state_.boards) {
    if (board.id == state_.activeBoardId) {
      return &board;
    }
  }
  return state_.boards.isEmpty() ? nullptr : &state_.boards.front();
}

void MainWindow::setPreviewStatus(const QString& title, int durationMs, bool playing) {
  if (!playing || title.trimmed().isEmpty()) {
    previewLabel_->setText(QStringLiteral("Preview stopped"));
    stopPreviewButton_->setEnabled(false);
    liveIndicator_->setVisible(false);
    return;
  }

  previewLabel_->setText(QStringLiteral("Playing: %1  [%2]").arg(title, formatDurationMs(durationMs)));
  stopPreviewButton_->setEnabled(true);
  liveIndicator_->setVisible(true);
}

void MainWindow::setSelectedCell(int cellIndex) {
  selectedCellIndex_ = cellIndex;
  for (int index = 0; index < cellButtons_.size(); ++index) {
    if (!cellButtons_[index]) {
      continue;
    }
    const bool selected = index == selectedCellIndex_;
    cellButtons_[index]->setStyleSheet(selected ? QStringLiteral("border: 2px solid #1e88e5;") : QString());
  }
}

void MainWindow::openYouTubeDialog() {
  YouTubeSearchDialog dialog(this);
  dialog.exec();
}

void MainWindow::handleCellClick(int cellIndex, const QString& soundId) {
  setSelectedCell(cellIndex);

  auto* currentItem = libraryList_->currentItem();
  if (currentItem && onAssignSoundToCell) {
    const QString newSoundId = currentItem->data(Qt::UserRole).toString();
    const QString itemText = currentItem->text();
    onAssignSoundToCell(newSoundId, cellIndex);
    statusLabel_->setText(QStringLiteral("Assigned \"%1\" to the selected cell.").arg(itemText));
    return;
  }

  if (!soundId.isEmpty()) {
    if (onPlaySound) {
      onPlaySound(soundId);
    }
    return;
  }

  QMessageBox actionDialog(this);
  actionDialog.setWindowTitle(QStringLiteral("Assign Cell"));
  actionDialog.setText(QStringLiteral("This cell is empty. Choose how to fill it."));
  auto* importButton = actionDialog.addButton(QStringLiteral("Import Sound"), QMessageBox::ActionRole);
  auto* youtubeButton = actionDialog.addButton(QStringLiteral("YouTube Search"), QMessageBox::ActionRole);
  actionDialog.addButton(QMessageBox::Cancel);
  actionDialog.exec();

  if (actionDialog.clickedButton() == importButton) {
    if (onImportSound) {
      onImportSound(cellIndex);
    }
  } else if (actionDialog.clickedButton() == youtubeButton) {
    openYouTubeDialog();
  }
}

void MainWindow::rebuild() {
  rebuildingUi_ = true;
  const QSignalBlocker boardSelectorBlock(boardSelector_);
  const QSignalBlocker volumeRemoteBlock(volumeRemoteSlider_);
  const QSignalBlocker volumeLocalBlock(volumeLocalSlider_);
  const QSignalBlocker muteOnClientBlock(muteOnClientCheckbox_);
  const QSignalBlocker muteMyselfBlock(muteMyselfDuringPlaybackCheckbox_);
  const QSignalBlocker showHotkeysBlock(showHotkeysOnButtonsCheckbox_);
  const QSignalBlocker disableHotkeysBlock(disableHotkeysCheckbox_);
  const QSignalBlocker rowsBlock(rowsSpin_);
  const QSignalBlocker colsBlock(colsSpin_);

  boardSelector_->clear();
  for (const BoardRecord& board : state_.boards) {
    boardSelector_->addItem(board.name, board.id);
    if (board.id == state_.activeBoardId) {
      boardSelector_->setCurrentIndex(boardSelector_->count() - 1);
    }
  }

  if (boardSelector_->currentIndex() < 0 && boardSelector_->count() > 0) {
    boardSelector_->setCurrentIndex(0);
  }

  freesoundApiKey_->setText(state_.config.freesoundApiKey);
  volumeRemoteSlider_->setValue(state_.config.volumeRemote);
  volumeLocalSlider_->setValue(state_.config.volumeLocal);
  muteOnClientCheckbox_->setChecked(!state_.config.playbackLocal);
  muteMyselfDuringPlaybackCheckbox_->setChecked(state_.config.muteMyselfDuringPlayback);
  showHotkeysOnButtonsCheckbox_->setChecked(state_.config.showHotkeysOnButtons);
  disableHotkeysCheckbox_->setChecked(!state_.config.globalHotkeysEnabled);

  const BoardRecord* board = activeBoard();
  rowsSpin_->setValue(board ? qMax(1, board->rows) : 1);
  colsSpin_->setValue(board ? qMax(1, board->cols) : 1);
  rebuildingUi_ = false;

  cellButtons_.clear();
  deleteButtons_.clear();

  while (QLayoutItem* item = gridLayout_->takeAt(0)) {
    delete item->widget();
    delete item;
  }

  if (board) {
    const int safeCols = qMax(1, board->cols);
    for (int index = 0; index < board->cells.size(); ++index) {
      const int row = (index / safeCols) * 2;
      const int col = index % safeCols;
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

      auto* button = new QPushButton(buildCellButtonLabel(board->cells[index], label), this);
      button->setMinimumHeight(72);
      gridLayout_->addWidget(button, row, col);
      cellButtons_.push_back(button);
      connect(button, &QPushButton::clicked, this, [this, index, soundId]() {
        handleCellClick(index, soundId);
      });

      auto* deleteButton = new QPushButton(QStringLiteral("✕"), this);
      deleteButton->setMaximumWidth(30);
      deleteButton->setMaximumHeight(24);
      deleteButton->setStyleSheet(QStringLiteral("QPushButton { color: #999; border: 1px solid #ddd; border-radius: 4px; padding: 0px; }"));
      gridLayout_->addWidget(deleteButton, row + 1, col, Qt::AlignHCenter);
      deleteButtons_.push_back(deleteButton);
      connect(deleteButton, &QPushButton::clicked, this, [this, index]() {
        if (onAssignSoundToCell) {
          onAssignSoundToCell(QString(), index);
          statusLabel_->setText(QStringLiteral("Cell cleared."));
        }
      });
    }
  }

  libraryList_->clear();
  for (const SoundRecord& sound : state_.library) {
    auto* item = new QListWidgetItem(QStringLiteral("%1 [%2]").arg(sound.displayName, sound.sourceType), libraryList_);
    item->setData(Qt::UserRole, sound.soundId);
  }

  if (selectedCellIndex_ >= cellButtons_.size()) {
    selectedCellIndex_ = -1;
  }
  setSelectedCell(selectedCellIndex_);
}

}  // namespace rpsu
