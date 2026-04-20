#include "src/ui/main_window.h"
#include "src/ui/emoji_picker.h"

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
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMenu>
#include <QPixmap>
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

// ---------------------------------------------------------------------------
// Theme palettes
// ---------------------------------------------------------------------------

struct Theme {
  QString windowBg, surfaceBg, surfaceBorder;
  QString textPrimary, textMuted;
  QString inputBg, inputBorder;
  QString buttonBg, buttonBorder, buttonHover, buttonDisabled, buttonDisabledText;
  QString accentBg, accentBorder;
  QString tableAltRow, tableGridline, headerBg, headerText;
  QString previewBg, previewBorder;
  QString cellBorder, cellBg, cellHover, cellSelected;
  QString statusBg, statusBorder;
};

static Theme lightTheme() {
  Theme t;
  t.windowBg            = QStringLiteral("#f5f7fb");
  t.surfaceBg           = QStringLiteral("#ffffff");
  t.surfaceBorder       = QStringLiteral("#d7dfeb");
  t.textPrimary         = QStringLiteral("#1f2937");
  t.textMuted           = QStringLiteral("#556274");
  t.inputBg             = QStringLiteral("#ffffff");
  t.inputBorder         = QStringLiteral("#cfd8e3");
  t.buttonBg            = QStringLiteral("#ffffff");
  t.buttonBorder        = QStringLiteral("#c6d2e1");
  t.buttonHover         = QStringLiteral("#eef4fb");
  t.buttonDisabled      = QStringLiteral("#eef1f5");
  t.buttonDisabledText  = QStringLiteral("#8c97a8");
  t.accentBg            = QStringLiteral("#dcecff");
  t.accentBorder        = QStringLiteral("#b7cdee");
  t.tableAltRow         = QStringLiteral("#f9fbfe");
  t.tableGridline       = QStringLiteral("#ecf1f7");
  t.headerBg            = QStringLiteral("#eef3f9");
  t.headerText          = QStringLiteral("#2b3442");
  t.previewBg           = QStringLiteral("#eef4fb");
  t.previewBorder       = QStringLiteral("#d3dfed");
  t.cellBorder          = QStringLiteral("#c6d2e1");
  t.cellBg              = QStringLiteral("#ffffff");
  t.cellHover           = QStringLiteral("#eef4fb");
  t.cellSelected        = QStringLiteral("#1e88e5");
  t.statusBg            = QStringLiteral("#f0f4fa");
  t.statusBorder        = QStringLiteral("#d3dfed");
  return t;
}

// Deep blue-slate dark mode — not pitch black
static Theme darkTheme() {
  Theme t;
  t.windowBg            = QStringLiteral("#0f1623");
  t.surfaceBg           = QStringLiteral("#162033");
  t.surfaceBorder       = QStringLiteral("#253045");
  t.textPrimary         = QStringLiteral("#e2e8f4");
  t.textMuted           = QStringLiteral("#7a8fad");
  t.inputBg             = QStringLiteral("#1a2640");
  t.inputBorder         = QStringLiteral("#2e4060");
  t.buttonBg            = QStringLiteral("#1e2f4a");
  t.buttonBorder        = QStringLiteral("#2e4060");
  t.buttonHover         = QStringLiteral("#243859");
  t.buttonDisabled      = QStringLiteral("#151f30");
  t.buttonDisabledText  = QStringLiteral("#4a5a72");
  t.accentBg            = QStringLiteral("#1a3a5c");
  t.accentBorder        = QStringLiteral("#2d5a8e");
  t.tableAltRow         = QStringLiteral("#131d2e");
  t.tableGridline       = QStringLiteral("#1e2d42");
  t.headerBg            = QStringLiteral("#1a2a40");
  t.headerText          = QStringLiteral("#c8d3e8");
  t.previewBg           = QStringLiteral("#111b2c");
  t.previewBorder       = QStringLiteral("#1e3050");
  t.cellBorder          = QStringLiteral("#2a3d58");
  t.cellBg              = QStringLiteral("#162033");
  t.cellHover           = QStringLiteral("#1e3050");
  t.cellSelected        = QStringLiteral("#3b82f6");
  t.statusBg            = QStringLiteral("#0d1620");
  t.statusBorder        = QStringLiteral("#1a2840");
  return t;
}

// ---------------------------------------------------------------------------
// Stylesheet helpers
// ---------------------------------------------------------------------------

static QString dialogStyleSheet(const Theme& t) {
  return QStringLiteral(
    "QDialog { background: %1; }"
    "QLabel  { color: %2; }"
    "QLineEdit { padding: 8px 10px; border: 1px solid %3; border-radius: 8px; background: %4; color: %2; }"
    "QPushButton { padding: 8px 14px; border: 1px solid %5; border-radius: 8px; background: %6; color: %2; }"
    "QPushButton:hover { background: %7; }"
    "QPushButton:disabled { color: %8; background: %9; }"
    "QTableWidget { background: %10; border: 1px solid %11; border-radius: 10px; gridline-color: %12; color: %2; }"
    "QHeaderView::section { background: %13; padding: 8px; border: none; border-bottom: 1px solid %11; color: %14; font-weight: 600; }"
  ).arg(t.windowBg, t.textPrimary, t.inputBorder, t.inputBg,
       t.buttonBorder, t.buttonBg, t.buttonHover,
       t.buttonDisabledText, t.buttonDisabled,
       t.surfaceBg, t.surfaceBorder, t.tableGridline,
       t.headerBg, t.headerText);
}

static QString tableItemStyleSheet(const Theme& t) {
  return QStringLiteral(
    "QTableWidget::item { padding: 10px 8px; border-bottom: 1px solid %1; color: %2; }"
    "QTableWidget::item:selected { background: %3; color: %2; }"
    "QTableWidget { alternate-background-color: %4; }"
  ).arg(t.tableGridline, t.textPrimary, t.accentBg, t.tableAltRow);
}

// ---------------------------------------------------------------------------
// YouTube Search Dialog
// ---------------------------------------------------------------------------

namespace {

QString formatDuration(int seconds) {
  if (seconds <= 0) return QStringLiteral("--:--");
  const int minutes   = seconds / 60;
  const int remaining = seconds % 60;
  return QStringLiteral("%1:%2").arg(minutes).arg(remaining, 2, 10, QLatin1Char('0'));
}

QString formatDurationMs(int durationMs) {
  return formatDuration(qMax(0, durationMs / 1000));
}

class YouTubeSearchDialog : public QDialog {
 public:
  explicit YouTubeSearchDialog(MainWindow* owner, bool darkMode)
      : QDialog(owner), owner_(owner), darkMode_(darkMode) {

    setWindowTitle(QStringLiteral("YouTube Search"));
    resize(980, 640);

    const Theme t = darkMode_ ? darkTheme() : lightTheme();
    setStyleSheet(dialogStyleSheet(t));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(14);

    // Title row with YouTube logo
    auto* titleRow    = new QHBoxLayout();
    auto* ytIconLabel = new QLabel(this);
    {
      QPixmap px(QStringLiteral(":/assets/youtube-icon.svg"));
      if (!px.isNull())
        ytIconLabel->setPixmap(px.scaled(32, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    auto* titleLabel = new QLabel(QStringLiteral("YouTube Search"), this);
    titleLabel->setStyleSheet(
      QStringLiteral("font-size: 20px; font-weight: 700; color: %1;").arg(t.textPrimary)
    );
    titleRow->addWidget(ytIconLabel);
    titleRow->addSpacing(8);
    titleRow->addWidget(titleLabel);
    titleRow->addStretch(1);
    root->addLayout(titleRow);

    // Search row
    auto* topRow    = new QHBoxLayout();
    queryEdit_      = new QLineEdit(this);
    queryEdit_->setPlaceholderText(QStringLiteral("Search YouTube"));
    searchButton_   = new QPushButton(QStringLiteral("Search"), this);
    loadMoreButton_ = new QPushButton(QStringLiteral("Load More"), this);
    loadMoreButton_->setEnabled(false);
    topRow->addWidget(queryEdit_, 1);
    topRow->addWidget(searchButton_);
    topRow->addWidget(loadMoreButton_);
    root->addLayout(topRow);

    statusLabel_ = new QLabel(QStringLiteral("Enter a search term and press Search."), this);
    statusLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(t.textMuted));
    root->addWidget(statusLabel_);

    // Results table
    resultsTable_ = new QTableWidget(this);
    resultsTable_->setColumnCount(4);
    resultsTable_->setHorizontalHeaderLabels({
      QStringLiteral("Title"), QStringLiteral("Length"),
      QStringLiteral("Channel"), QStringLiteral("Actions")
    });
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
    resultsTable_->setStyleSheet(tableItemStyleSheet(t));
    root->addWidget(resultsTable_, 1);

    // Bottom buttons
    auto* buttons   = new QHBoxLayout();
    downloadButton_ = new QPushButton(QStringLiteral("Download Selected"), this);
    closeButton_    = new QPushButton(QStringLiteral("Close"), this);
    downloadButton_->setStyleSheet(
      QStringLiteral("QPushButton { background: %1; border-color: %2; font-weight: 600; }")
        .arg(t.accentBg, t.accentBorder)
    );
    buttons->addStretch(1);
    buttons->addWidget(downloadButton_);
    buttons->addWidget(closeButton_);
    root->addLayout(buttons);

    connect(searchButton_,   &QPushButton::clicked,           this, [this]() { runSearch(); });
    connect(loadMoreButton_, &QPushButton::clicked,           this, [this]() { loadMore(); });
    connect(queryEdit_,      &QLineEdit::returnPressed,       this, [this]() { runSearch(); });
    connect(downloadButton_, &QPushButton::clicked,           this, [this]() { downloadSelected(); });
    connect(closeButton_,    &QPushButton::clicked,           this, &QDialog::reject);
    connect(resultsTable_,   &QTableWidget::cellDoubleClicked,this, [this](int row, int) {
      resultsTable_->selectRow(row);
      downloadSelected();
    });
  }

 private:
  template <typename Fn>
  auto runWithLoading(const QString& msg, Fn&& fn) -> decltype(fn()) {
    setBusy(true);
    loadingTick_ = 0;
    auto future = std::async(std::launch::async, std::forward<Fn>(fn));
    while (future.wait_for(std::chrono::milliseconds(80)) != std::future_status::ready) {
      updateLoadingMessage(msg);
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    updateLoadingMessage(msg);
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

  void updateLoadingMessage(const QString& base) {
    static const char* frames[] = { ".", "..", "...", "...." };
    statusLabel_->setText(base + QString::fromLatin1(frames[loadingTick_ % 4]));
    ++loadingTick_;
  }

  void runSearch() {
    const QString query = queryEdit_->text().trimmed();
    if (query.isEmpty()) {
      statusLabel_->setText(QStringLiteral("Enter a search term first."));
      return;
    }
    lastQuery_    = query;
    currentLimit_ = kPageSize;
    refreshResults(true);
  }

  void loadMore() {
    if (lastQuery_.isEmpty()) return;
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

    const Theme t = darkMode_ ? darkTheme() : lightTheme();
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

      auto* actionHost   = new QWidget(resultsTable_);
      actionHost->setStyleSheet(QStringLiteral("background: transparent;"));
      auto* actionLayout = new QHBoxLayout(actionHost);
      actionLayout->setContentsMargins(6, 4, 6, 4);
      actionLayout->setSpacing(6);

      auto* previewButton     = new QPushButton(QStringLiteral("Preview"),  actionHost);
      auto* downloadRowButton = new QPushButton(QStringLiteral("Download"), actionHost);
      previewButton->setCursor(Qt::PointingHandCursor);
      downloadRowButton->setCursor(Qt::PointingHandCursor);
      previewButton->setStyleSheet(
        QStringLiteral("QPushButton { padding: 5px 10px; border-radius: 6px; }")
      );
      downloadRowButton->setStyleSheet(
        QStringLiteral("QPushButton { padding: 5px 10px; border-radius: 6px; background: %1; border-color: %2; font-weight: 600; }")
          .arg(t.accentBg, t.accentBorder)
      );

      connect(previewButton, &QPushButton::clicked, this, [this, result]() {
        if (!owner_->onYouTubePreview) {
          statusLabel_->setText(QStringLiteral("Preview is not available."));
          return;
        }
        QString error;
        const bool ok = runWithLoading(QStringLiteral("Preparing preview"), [this, &result, &error]() {
          return owner_->onYouTubePreview(result, &error);
        });
        if (!ok) {
          statusLabel_->setText(error.isEmpty() ? QStringLiteral("Preview failed.") : error);
        } else {
          statusLabel_->setText(QStringLiteral("Playing preview: %1").arg(result.title));
        }
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

    if (resetSelection && resultsTable_->rowCount() > 0) resultsTable_->selectRow(0);

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
    if (owner_->selectedCellIndex() >= 0 && owner_->onAssignSoundToCell)
      owner_->onAssignSoundToCell(soundId, owner_->selectedCellIndex());
    accept();
  }

  MainWindow*   owner_;
  bool          darkMode_;
  QLineEdit*    queryEdit_      = nullptr;
  QLabel*       statusLabel_    = nullptr;
  QTableWidget* resultsTable_   = nullptr;
  QVector<YouTubeSearchResult> results_;
  QPushButton*  searchButton_   = nullptr;
  QPushButton*  loadMoreButton_ = nullptr;
  QPushButton*  downloadButton_ = nullptr;
  QPushButton*  closeButton_    = nullptr;
  bool          busy_           = false;
  int           loadingTick_    = 0;
  QString       lastQuery_;
  int           currentLimit_   = 0;
  static constexpr int kPageSize = 12;
};

}  // anonymous namespace

// ---------------------------------------------------------------------------
// MainWindow constructor
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
  setWindowTitle(QStringLiteral("RP Soundboard Ultimate"));
  resize(980, 660);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  // ── Top bar ──────────────────────────────────────────────────────────────
  auto* topBarFrame = new QFrame(this);
  topBarFrame->setObjectName(QStringLiteral("topBarFrame"));
  auto* topBar = new QHBoxLayout(topBarFrame);
  topBar->setContentsMargins(12, 8, 12, 8);
  topBar->setSpacing(8);

  // fALECX brand button
  auto* brandButton = new QToolButton(topBarFrame);
  brandButton->setObjectName(QStringLiteral("brandButton"));
  brandButton->setToolTip(QStringLiteral("Visit fALECX on Twitch"));
  brandButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  brandButton->setIconSize(QSize(24, 24));
  brandButton->setText(QStringLiteral("fALECX"));
  {
    QPixmap px(QStringLiteral(":/assets/fALECX Logo.png"));
    if (!px.isNull()) {
      brandButton->setIcon(px.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      brandButton->setText(QStringLiteral(""));
    }
  }
  topBar->addWidget(brandButton);

  topBar->addWidget(new QLabel(QStringLiteral("Board"), topBarFrame));
  boardSelector_ = new QComboBox(topBarFrame);
  boardSelector_->setObjectName(QStringLiteral("boardSelector"));
  topBar->addWidget(boardSelector_, 1);

  importButton_ = new QPushButton(QStringLiteral("Import Sound"), topBarFrame);
  importButton_->setObjectName(QStringLiteral("importButton"));
  topBar->addWidget(importButton_);

  // YouTube button with logo
  youtubeButton_ = new QPushButton(QStringLiteral(" YouTube Search"), topBarFrame);
  youtubeButton_->setObjectName(QStringLiteral("youtubeButton"));
  {
    QPixmap px(QStringLiteral(":/assets/youtube-icon.svg"));
    if (!px.isNull())
      youtubeButton_->setIcon(px.scaled(20, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  topBar->addWidget(youtubeButton_);

  // Freesound key with icon
  auto* freesoundWidget = new QWidget(topBarFrame);
  auto* freesoundRow    = new QHBoxLayout(freesoundWidget);
  freesoundRow->setContentsMargins(0, 0, 0, 0);
  freesoundRow->setSpacing(4);
  auto* fsIconLabel = new QLabel(freesoundWidget);
  fsIconLabel->setObjectName(QStringLiteral("fsIconLabel"));
  {
    QPixmap px(QStringLiteral(":/assets/freesound-icon.svg"));
    if (!px.isNull())
      fsIconLabel->setPixmap(px.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  freesoundApiKey_ = new QLineEdit(freesoundWidget);
  freesoundApiKey_->setObjectName(QStringLiteral("freesoundApiKey"));
  freesoundApiKey_->setPlaceholderText(QStringLiteral("Freesound API key"));
  freesoundRow->addWidget(fsIconLabel);
  freesoundRow->addWidget(freesoundApiKey_);
  topBar->addWidget(freesoundWidget, 1);

  // Dark mode toggle (top-right)
  darkModeButton_ = new QToolButton(topBarFrame);
  darkModeButton_->setObjectName(QStringLiteral("darkModeButton"));
  darkModeButton_->setCheckable(true);
  darkModeButton_->setToolTip(QStringLiteral("Toggle dark / light mode"));
  darkModeButton_->setText(QStringLiteral("🌙"));
  darkModeButton_->setFixedSize(36, 36);
  topBar->addWidget(darkModeButton_);

  root->addWidget(topBarFrame);

  // ── Status strip ─────────────────────────────────────────────────────────
  auto* statusFrame = new QFrame(this);
  statusFrame->setObjectName(QStringLiteral("statusFrame"));
  auto* statusLayout = new QHBoxLayout(statusFrame);
  statusLayout->setContentsMargins(14, 5, 14, 5);
  statusLabel_ = new QLabel(
    QStringLiteral("Select a cell, then import, search YouTube, or assign from the library."),
    statusFrame
  );
  statusLabel_->setObjectName(QStringLiteral("statusLabel"));
  statusLayout->addWidget(statusLabel_);
  root->addWidget(statusFrame);

  // ── Main content ─────────────────────────────────────────────────────────
  auto* contentWidget = new QWidget(this);
  auto* contentLayout = new QVBoxLayout(contentWidget);
  contentLayout->setContentsMargins(12, 10, 12, 12);
  contentLayout->setSpacing(10);

  // Settings panel
  settingsFrame_ = new QFrame(contentWidget);
  settingsFrame_->setObjectName(QStringLiteral("settingsFrame"));
  settingsFrame_->setFrameShape(QFrame::StyledPanel);
  auto* settingsLayout = new QVBoxLayout(settingsFrame_);
  settingsLayout->setContentsMargins(14, 10, 14, 10);
  settingsLayout->setSpacing(8);

  auto* settingsTitle = new QLabel(QStringLiteral("Settings"), settingsFrame_);
  settingsTitle->setObjectName(QStringLiteral("settingsTitle"));
  settingsTitle->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 13px;"));
  settingsLayout->addWidget(settingsTitle);

  auto* settingsRows = new QHBoxLayout();
  auto* volumeLayout = new QVBoxLayout();

  auto* volumeRemoteRow = new QHBoxLayout();
  volumeRemoteRow->addWidget(new QLabel(QStringLiteral("Volume Remote"), settingsFrame_));
  volumeRemoteSlider_ = new QSlider(Qt::Horizontal, settingsFrame_);
  volumeRemoteSlider_->setRange(0, 100);
  volumeRemoteSlider_->setPageStep(10);
  volumeRemoteRow->addWidget(volumeRemoteSlider_, 1);
  volumeLayout->addLayout(volumeRemoteRow);

  auto* volumeLocalRow = new QHBoxLayout();
  volumeLocalRow->addWidget(new QLabel(QStringLiteral("Volume Local"), settingsFrame_));
  volumeLocalSlider_ = new QSlider(Qt::Horizontal, settingsFrame_);
  volumeLocalSlider_->setRange(0, 100);
  volumeLocalSlider_->setPageStep(10);
  volumeLocalRow->addWidget(volumeLocalSlider_, 1);
  volumeLayout->addLayout(volumeLocalRow);
  settingsRows->addLayout(volumeLayout, 3);

  auto* togglesLayout = new QVBoxLayout();
  muteOnClientCheckbox_             = new QCheckBox(QStringLiteral("Mute on my client"),           settingsFrame_);
  muteMyselfDuringPlaybackCheckbox_ = new QCheckBox(QStringLiteral("Mute myself during playback"), settingsFrame_);
  showHotkeysOnButtonsCheckbox_     = new QCheckBox(QStringLiteral("Show hotkeys on buttons"),     settingsFrame_);
  disableHotkeysCheckbox_           = new QCheckBox(QStringLiteral("Disable hotkeys"),             settingsFrame_);
  togglesLayout->addWidget(muteOnClientCheckbox_);
  togglesLayout->addWidget(muteMyselfDuringPlaybackCheckbox_);
  togglesLayout->addWidget(showHotkeysOnButtonsCheckbox_);
  togglesLayout->addWidget(disableHotkeysCheckbox_);
  togglesLayout->addStretch(1);
  settingsRows->addLayout(togglesLayout, 2);

  auto* sizeLayout = new QHBoxLayout();
  sizeLayout->addWidget(new QLabel(QStringLiteral("Rows:"), settingsFrame_));
  rowsSpin_ = new QSpinBox(settingsFrame_);
  rowsSpin_->setRange(1, 50);
  sizeLayout->addWidget(rowsSpin_);
  sizeLayout->addSpacing(8);
  sizeLayout->addWidget(new QLabel(QStringLiteral("Columns:"), settingsFrame_));
  colsSpin_ = new QSpinBox(settingsFrame_);
  colsSpin_->setRange(1, 50);
  sizeLayout->addWidget(colsSpin_);
  sizeLayout->addStretch(1);

  settingsLayout->addLayout(settingsRows);
  settingsLayout->addLayout(sizeLayout);
  contentLayout->addWidget(settingsFrame_);

  // Preview bar
  previewBar_ = new QFrame(contentWidget);
  previewBar_->setObjectName(QStringLiteral("previewBar"));
  auto* previewLayout = new QHBoxLayout(previewBar_);
  previewLayout->setContentsMargins(14, 8, 14, 8);
  liveIndicator_ = new QLabel(previewBar_);
  liveIndicator_->setFixedSize(12, 12);
  liveIndicator_->setStyleSheet(QStringLiteral("background-color: #ef4444; border-radius: 6px;"));
  liveIndicator_->setVisible(false);
  previewLabel_ = new QLabel(QStringLiteral("Preview stopped"), previewBar_);
  previewLabel_->setObjectName(QStringLiteral("previewLabel"));
  stopPreviewButton_ = new QPushButton(QStringLiteral("Stop Preview"), previewBar_);
  stopPreviewButton_->setObjectName(QStringLiteral("stopPreviewButton"));
  stopPreviewButton_->setEnabled(false);
  previewLayout->addWidget(liveIndicator_);
  previewLayout->addSpacing(8);
  previewLayout->addWidget(previewLabel_, 1);
  previewLayout->addWidget(stopPreviewButton_);
  contentLayout->addWidget(previewBar_);

  // Sound grid + library
  auto* body    = new QHBoxLayout();
  gridHost_     = new QWidget(contentWidget);
  gridHost_->setObjectName(QStringLiteral("gridHost"));
  gridLayout_   = new QGridLayout(gridHost_);
  gridLayout_->setSpacing(6);
  libraryList_  = new QListWidget(contentWidget);
  libraryList_->setObjectName(QStringLiteral("libraryList"));
  libraryList_->setMinimumWidth(280);
  body->addWidget(gridHost_, 2);
  body->addWidget(libraryList_, 1);
  contentLayout->addLayout(body, 1);

  root->addWidget(contentWidget, 1);

  // ── Initial theme ────────────────────────────────────────────────────────
  applyTheme();

  // ── Connections ──────────────────────────────────────────────────────────
  connect(darkModeButton_, &QToolButton::toggled, this, [this](bool on) {
    darkMode_ = on;
    darkModeButton_->setText(on ? QStringLiteral("☀️") : QStringLiteral("🌙"));
    applyTheme();
  });

  connect(brandButton, &QToolButton::clicked, this, []() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://twitch.tv/fALECX")));
  });

  connect(boardSelector_, &QComboBox::currentTextChanged, this, [this]() {
    if (onBoardSelected) onBoardSelected(boardSelector_->currentData().toString());
  });

  connect(importButton_, &QPushButton::clicked, this, [this]() {
    if (onImportSound) onImportSound(selectedCellIndex_);
  });

  connect(youtubeButton_, &QPushButton::clicked, this, [this]() { openYouTubeDialog(); });

  connect(libraryList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    if (!item) return;
    if (selectedCellIndex_ >= 0)
      statusLabel_->setText(
        QStringLiteral("Ready to assign \"%1\" to the selected cell. Double-click to confirm.").arg(item->text())
      );
  });

  connect(libraryList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
    if (!item) return;
    const QString sid  = item->data(Qt::UserRole).toString();
    const QString text = item->text();
    if (selectedCellIndex_ >= 0 && onAssignSoundToCell) {
      onAssignSoundToCell(sid, selectedCellIndex_);
      statusLabel_->setText(QStringLiteral("Assigned \"%1\" to the selected cell.").arg(text));
      return;
    }
    if (onPlaySound) onPlaySound(sid);
  });

  connect(freesoundApiKey_, &QLineEdit::editingFinished, this, [this]() {
    if (onFreesoundApiKeyChanged) onFreesoundApiKeyChanged(freesoundApiKey_->text().trimmed());
  });

  connect(volumeRemoteSlider_, &QSlider::valueChanged, this, [this](int v) {
    if (!rebuildingUi_ && onVolumeRemoteChanged) onVolumeRemoteChanged(v);
  });
  connect(volumeLocalSlider_, &QSlider::valueChanged, this, [this](int v) {
    if (!rebuildingUi_ && onVolumeLocalChanged) onVolumeLocalChanged(v);
  });
  connect(muteOnClientCheckbox_, &QCheckBox::toggled, this, [this](bool c) {
    if (!rebuildingUi_ && onPlaybackLocalChanged) onPlaybackLocalChanged(!c);
  });
  connect(muteMyselfDuringPlaybackCheckbox_, &QCheckBox::toggled, this, [this](bool c) {
    if (!rebuildingUi_ && onMuteMyselfDuringPlaybackChanged) onMuteMyselfDuringPlaybackChanged(c);
  });
  connect(showHotkeysOnButtonsCheckbox_, &QCheckBox::toggled, this, [this](bool c) {
    if (!rebuildingUi_ && onShowHotkeysOnButtonsChanged) onShowHotkeysOnButtonsChanged(c);
  });
  connect(disableHotkeysCheckbox_, &QCheckBox::toggled, this, [this](bool c) {
    if (!rebuildingUi_ && onGlobalHotkeysEnabledChanged) onGlobalHotkeysEnabledChanged(!c);
  });
  connect(rowsSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int rows) {
    if (!rebuildingUi_ && onActiveBoardSizeChanged) onActiveBoardSizeChanged(rows, colsSpin_->value());
  });
  connect(colsSpin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int cols) {
    if (!rebuildingUi_ && onActiveBoardSizeChanged) onActiveBoardSizeChanged(rowsSpin_->value(), cols);
  });
  connect(stopPreviewButton_, &QPushButton::clicked, this, [this]() {
    if (onStopPreview) onStopPreview();
  });
}

// ---------------------------------------------------------------------------
// applyTheme
// ---------------------------------------------------------------------------

void MainWindow::applyTheme() {
  const Theme t = darkMode_ ? darkTheme() : lightTheme();

  // Qt stylesheet: %1..%21 placeholders, filled in order at the bottom
  setStyleSheet(QStringLiteral(
    "QWidget { background: %1; color: %2; }"
    "#topBarFrame { background: %3; border-bottom: 1px solid %4; }"
    "#statusFrame { background: %5; border-bottom: 1px solid %6; }"
    "#statusLabel  { color: %7; font-size: 12px; }"

    "QPushButton {"
    "  padding: 7px 14px; border: 1px solid %8; border-radius: 8px; background: %9; color: %2; }"
    "QPushButton:hover    { background: %10; }"
    "QPushButton:disabled { color: %11; background: %12; }"

    "#youtubeButton { background: #e02020; border-color: #b31b1b; color: white; font-weight: 600; }"
    "#youtubeButton:hover { background: #c81a1a; }"
    "#importButton { font-weight: 600; }"

    "QToolButton { border: 1px solid %8; border-radius: 8px; padding: 5px 8px; background: %9; color: %2; }"
    "QToolButton:hover   { background: %10; }"
    "QToolButton:checked { background: %13; border-color: %14; }"
    "#brandButton { border-color: rgba(145,71,255,0.30); background: rgba(145,71,255,0.08); }"
    "#brandButton:hover  { background: rgba(145,71,255,0.20); }"
    "#darkModeButton { font-size: 16px; border-radius: 18px; }"

    "QComboBox { padding: 6px 10px; border: 1px solid %8; border-radius: 8px; background: %15; color: %2; }"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView { background: %15; color: %2; border: 1px solid %8; selection-background-color: %13; }"

    "QLineEdit { padding: 7px 10px; border: 1px solid %8; border-radius: 8px; background: %15; color: %2; }"

    "#settingsFrame { background: %3; border: 1px solid %4; border-radius: 10px; }"
    "QCheckBox { color: %2; spacing: 6px; }"
    "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid %8; border-radius: 3px; background: %15; }"
    "QCheckBox::indicator:checked { background: %16; border-color: %16; }"

    "QSpinBox { padding: 5px 8px; border: 1px solid %8; border-radius: 8px; background: %15; color: %2; }"

    "QSlider::groove:horizontal { height: 4px; background: %4; border-radius: 2px; }"
    "QSlider::handle:horizontal { width: 14px; height: 14px; background: %16; border-radius: 7px; margin: -5px 0; }"
    "QSlider::sub-page:horizontal { background: %16; border-radius: 2px; }"

    "#previewBar { background: %17; border: 1px solid %18; border-radius: 10px; }"
    "#previewLabel { color: %7; font-size: 12px; }"

    "#gridHost QPushButton { min-height: 72px; border: 1px solid %19; border-radius: 8px; background: %20; color: %2; font-size: 12px; }"
    "#gridHost QPushButton:hover   { background: %21; border-color: %16; }"
    "#gridHost QPushButton:pressed { background: %13; }"

    "#libraryList { background: %3; border: 1px solid %4; border-radius: 10px; color: %2; }"
    "#libraryList::item { padding: 6px 10px; border-radius: 6px; }"
    "#libraryList::item:selected { background: %13; }"
    "#libraryList::item:hover    { background: %10; }"
  )
  .arg(t.windowBg,           //  1
       t.textPrimary,        //  2
       t.surfaceBg,          //  3
       t.surfaceBorder,      //  4
       t.statusBg,           //  5
       t.statusBorder,       //  6
       t.textMuted,          //  7
       t.buttonBorder,       //  8
       t.buttonBg,           //  9
       t.buttonHover,        // 10
       t.buttonDisabledText, // 11
       t.buttonDisabled,     // 12
       t.accentBg,           // 13
       t.accentBorder,       // 14
       t.inputBg,            // 15
       t.cellSelected,       // 16
       t.previewBg,          // 17
       t.previewBorder,      // 18
       t.cellBorder,         // 19
       t.cellBg,             // 20
       t.cellHover)          // 21
  );

  setSelectedCell(selectedCellIndex_);
}

// ---------------------------------------------------------------------------
// Remaining MainWindow methods
// ---------------------------------------------------------------------------

void MainWindow::setState(const AppState& state) {
  state_ = state;
  rebuild();
}

QString MainWindow::buildCellButtonLabel(const Cell& cell, const QString& fallbackText) const {
  QString label = fallbackText;
  if (state_.config.showHotkeysOnButtons && !cell.hotkey.trimmed().isEmpty())
    label += QStringLiteral("\n[%1]").arg(cell.hotkey);
  return label;
}

const BoardRecord* MainWindow::activeBoard() const {
  for (const BoardRecord& board : state_.boards)
    if (board.id == state_.activeBoardId) return &board;
  return state_.boards.isEmpty() ? nullptr : &state_.boards.front();
}

void MainWindow::setPreviewStatus(const QString& title, int durationMs, bool playing) {
  if (!playing || title.trimmed().isEmpty()) {
    previewLabel_->setText(QStringLiteral("Preview stopped"));
    stopPreviewButton_->setEnabled(false);
    liveIndicator_->setVisible(false);
    return;
  }
  previewLabel_->setText(
    QStringLiteral("Playing: %1  [%2]").arg(title, formatDurationMs(durationMs))
  );
  stopPreviewButton_->setEnabled(true);
  liveIndicator_->setVisible(true);
}

void MainWindow::setSelectedCell(int cellIndex) {
  selectedCellIndex_ = cellIndex;
  const Theme t = darkMode_ ? darkTheme() : lightTheme();
  for (int i = 0; i < cellButtons_.size(); ++i) {
    if (!cellButtons_[i]) continue;
    cellButtons_[i]->setStyleSheet(
      (i == selectedCellIndex_)
        ? QStringLiteral("border: 2px solid %1 !important;").arg(t.cellSelected)
        : QString()
    );
  }
}

void MainWindow::openYouTubeDialog() {
  YouTubeSearchDialog dlg(this, darkMode_);
  dlg.exec();
}

void MainWindow::showRenameDialog(const QString& soundId) {
  for (const SoundRecord& sound : state_.library) {
    if (sound.soundId != soundId) {
      continue;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Rename Sound"));
    dialog.setModal(true);
    dialog.resize(400, 150);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    auto* label = new QLabel(QStringLiteral("Sound Name:"), &dialog);
    layout->addWidget(label);

    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setText(sound.displayName);
    layout->addWidget(nameEdit);

    auto* emojiLayout = new QHBoxLayout();
    auto* emojiLabel = new QLabel(QStringLiteral("Icon:"), &dialog);
    auto* emojiButton = new QPushButton(sound.icon, &dialog);
    emojiButton->setMaximumWidth(60);
    emojiButton->setStyleSheet(QStringLiteral("QPushButton { font-size: 24px; }"));
    emojiLayout->addWidget(emojiLabel);
    emojiLayout->addWidget(emojiButton);
    emojiLayout->addStretch(1);
    layout->addLayout(emojiLayout);

    QString selectedEmoji = sound.icon;
    connect(emojiButton, &QPushButton::clicked, &dialog, [this, &dialog, &selectedEmoji, emojiButton, sound]() {
      EmojiPicker picker(sound.icon, &dialog);
      if (picker.exec() == QDialog::Accepted) {
        selectedEmoji = picker.selectedEmoji();
        emojiButton->setText(selectedEmoji);
      }
    });

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
      const QString newName = nameEdit->text().trimmed();
      if (!newName.isEmpty()) {
        if (onSoundRenamed) {
          onSoundRenamed(soundId, newName);
        }
        if (selectedEmoji != sound.icon && onSoundEmojiChanged) {
          onSoundEmojiChanged(soundId, selectedEmoji);
        }
      }
    }
    return;
  }
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
    if (onPlaySound) onPlaySound(soundId);
    return;
  }

  auto* currentItem = libraryList_->currentItem();
  if (currentItem && onAssignSoundToCell) {
    onAssignSoundToCell(currentItem->data(Qt::UserRole).toString(), cellIndex);
    statusLabel_->setText(
      QStringLiteral("Assigned \"%1\" to the selected cell.").arg(currentItem->text())
    );
    return;
  }

  QMessageBox actionDialog(this);
  actionDialog.setWindowTitle(QStringLiteral("Assign Cell"));
  actionDialog.setText(QStringLiteral("This cell is empty. Choose how to fill it."));
  auto* importBtn  = actionDialog.addButton(QStringLiteral("Import Sound"),   QMessageBox::ActionRole);
  auto* youtubeBtn = actionDialog.addButton(QStringLiteral("YouTube Search"), QMessageBox::ActionRole);
  actionDialog.addButton(QMessageBox::Cancel);
  actionDialog.exec();

  if (actionDialog.clickedButton() == importBtn) {
    if (onImportSound) onImportSound(cellIndex);
  } else if (actionDialog.clickedButton() == youtubeBtn) {
    openYouTubeDialog();
  }
}

void MainWindow::rebuild() {
  rebuildingUi_ = true;
  const QSignalBlocker b0(boardSelector_),    b1(volumeRemoteSlider_),
                       b2(volumeLocalSlider_), b3(muteOnClientCheckbox_),
                       b4(muteMyselfDuringPlaybackCheckbox_),
                       b5(showHotkeysOnButtonsCheckbox_),
                       b6(disableHotkeysCheckbox_),
                       b7(rowsSpin_),          b8(colsSpin_);

  boardSelector_->clear();
  for (const BoardRecord& board : state_.boards) {
    boardSelector_->addItem(board.name, board.id);
    if (board.id == state_.activeBoardId)
      boardSelector_->setCurrentIndex(boardSelector_->count() - 1);
  }
  if (boardSelector_->currentIndex() < 0 && boardSelector_->count() > 0)
    boardSelector_->setCurrentIndex(0);

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
      QString label   = QStringLiteral("+");
      QString soundId;
      QString emoji = QStringLiteral("🔊");
      if (!board->cells[index].soundId.isEmpty()) {
        soundId = board->cells[index].soundId;
        for (const SoundRecord& sound : state_.library) {
          if (sound.soundId == soundId) {
            label = sound.displayName;
            emoji = sound.icon;
            break;
          }
          if (sound.soundId == soundId) { label = sound.displayName; break; }
        }
      }

      auto* cellWidget = new QWidget(this);
      auto* cellLayout = new QVBoxLayout(cellWidget);
      cellLayout->setContentsMargins(0, 0, 0, 0);
      cellLayout->setSpacing(4);

      auto* emojiButton = new QPushButton(emoji, this);
      emojiButton->setStyleSheet(
        QStringLiteral("QPushButton { font-size: 36px; border: none; background: transparent; padding: 8px; } "
                       "QPushButton:hover { background: rgba(0, 0, 0, 0.05); border-radius: 4px; }"));
      emojiButton->setMinimumHeight(60);
      cellLayout->addWidget(emojiButton, 0, Qt::AlignCenter);
      auto* button = new QPushButton(buildCellButtonLabel(board->cells[index], label), this);
      button->setMinimumHeight(40);
      button->setContextMenuPolicy(Qt::CustomContextMenu);
      cellLayout->addWidget(button);

      gridLayout_->addWidget(cellWidget, row, col);
      cellButtons_.push_back(button);

      const QString currentSoundId = soundId;
      connect(emojiButton, &QPushButton::clicked, this, [this, currentSoundId]() {
        if (currentSoundId.isEmpty()) {
          statusLabel_->setText(QStringLiteral("Assign a sound to the cell first."));
          return;
        }
        EmojiPicker picker(QString(), this);
        if (picker.exec() == QDialog::Accepted) {
          if (onSoundEmojiChanged) {
            onSoundEmojiChanged(currentSoundId, picker.selectedEmoji());
          }
        }
      });

      connect(button, &QWidget::customContextMenuRequested, this, [this, currentSoundId](const QPoint&) {
        if (currentSoundId.isEmpty()) {
          return;
        }
        QMenu contextMenu;
        contextMenu.addAction(QStringLiteral("Rename..."), this, [this, currentSoundId]() {
          showRenameDialog(currentSoundId);
        });
        contextMenu.exec(QCursor::pos());
      });

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
    auto* item = new QListWidgetItem(
      QStringLiteral("%1 [%2]").arg(sound.displayName, sound.sourceType), libraryList_
    );
    item->setData(Qt::UserRole, sound.soundId);
  }

  if (selectedCellIndex_ >= cellButtons_.size()) selectedCellIndex_ = -1;
  setSelectedCell(selectedCellIndex_);
}

}  // namespace rpsu
