#include "src/ui/main_window.h"
#include "src/ui/emoji_picker.h"

#include <atomic>
#include <chrono>
#include <future>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QEventLoop>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMenu>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
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

QString libraryMetaLabel(const SoundRecord& sound) {
  const QString source = sound.sourceType.trimmed().isEmpty() ? QStringLiteral("local") : sound.sourceType;
  return source.toUpper();
}

QString emojiFromUtf8(const char* value) {
  return QString::fromUtf8(value);
}

QString compactCellLabel(const QString& value, int rows, int cols) {
  const int density = qMax(rows, cols);
  const int limit = density >= 10 ? 18 : (density >= 7 ? 28 : 44);
  QString label = value.simplified();
  if (label.size() <= limit) {
    return label;
  }
  return label.left(qMax(1, limit - 3)).trimmed() + QStringLiteral("...");
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

    auto* statusRow = new QHBoxLayout();
    statusRow->setSpacing(8);
    statusLabel_ = new QLabel(QStringLiteral("Enter a search term and press Search."), this);
    statusLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(t.textMuted));
    cancelButton_ = new QPushButton(QStringLiteral("Cancel"), this);
    cancelButton_->setStyleSheet(
      QStringLiteral("QPushButton { padding: 4px 12px; border-radius: 6px;"
                     " background: #c0392b; border-color: #922b21; color: #fff; font-weight: 600; }"
                     "QPushButton:hover { background: #e74c3c; }"
                     "QPushButton:disabled { background: #7b241c; color: #d5a5a5; }")
    );
    cancelButton_->setVisible(false);
    statusRow->addWidget(statusLabel_, 1);
    statusRow->addWidget(cancelButton_);
    root->addLayout(statusRow);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 0);
    progressBar_->setTextVisible(false);
    progressBar_->setFixedHeight(5);
    progressBar_->setVisible(false);
    progressBar_->setStyleSheet(
      QStringLiteral("QProgressBar { border: none; background: %1; border-radius: 2px; }"
                     "QProgressBar::chunk { background: %2; border-radius: 2px; }")
        .arg(t.surfaceBorder, t.accentBorder)
    );
    root->addWidget(progressBar_);

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
    connect(cancelButton_,   &QPushButton::clicked,           this, [this]() {
      cancelRequested_.store(true);
      cancelButton_->setEnabled(false);
      statusLabel_->setText(QStringLiteral("Cancelling…"));
    });
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
      if (progressBar_->isVisible()) {
        const int pct = downloadProgress_.load();
        if (pct >= 0 && pct < 100) {
          if (progressBar_->maximum() != 100) progressBar_->setRange(0, 100);
          progressBar_->setValue(pct);
          static const char* frames[] = { ".", "..", "...", "...." };
          statusLabel_->setText(
            QStringLiteral("Downloading (%1%)").arg(pct)
            + QString::fromLatin1(frames[loadingTick_ % 4])
          );
          ++loadingTick_;
        } else if (pct == 100) {
          if (progressBar_->maximum() != 0) progressBar_->setRange(0, 0);
          static const char* frames[] = { ".", "..", "...", "...." };
          statusLabel_->setText(
            QStringLiteral("Converting to WAV")
            + QString::fromLatin1(frames[loadingTick_ % 4])
          );
          ++loadingTick_;
        } else {
          if (progressBar_->maximum() != 0) progressBar_->setRange(0, 0);
          updateLoadingMessage(msg);
        }
      } else {
        updateLoadingMessage(msg);
      }
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
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

    cancelRequested_.store(false);
    downloadProgress_.store(-1);
    cancelButton_->setEnabled(true);
    cancelButton_->setVisible(true);
    progressBar_->setRange(0, 0);
    progressBar_->setVisible(true);

    QString error;
    const QString soundId = runWithLoading(
      QStringLiteral("Downloading audio"),
      [this, index, &error]() {
        return owner_->onYouTubeDownload(results_[index], &error, &cancelRequested_, &downloadProgress_);
      }
    );

    cancelButton_->setVisible(false);
    progressBar_->setVisible(false);
    progressBar_->setRange(0, 100);
    downloadProgress_.store(-1);

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
  QProgressBar* progressBar_    = nullptr;
  QTableWidget* resultsTable_   = nullptr;
  QVector<YouTubeSearchResult> results_;
  QPushButton*  searchButton_   = nullptr;
  QPushButton*  loadMoreButton_ = nullptr;
  QPushButton*  downloadButton_ = nullptr;
  QPushButton*  closeButton_    = nullptr;
  QPushButton*  cancelButton_   = nullptr;
  bool          busy_           = false;
  int           loadingTick_    = 0;
  QString       lastQuery_;
  int           currentLimit_   = 0;
  std::atomic<bool> cancelRequested_{false};
  std::atomic<int>  downloadProgress_{-1};
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

  boardSelector_ = new QComboBox(topBarFrame);
  boardSelector_->setObjectName(QStringLiteral("boardSelector"));
  addBoardButton_ = new QToolButton(topBarFrame);
  addBoardButton_->setObjectName(QStringLiteral("addBoardButton"));
  addBoardButton_->setText(QStringLiteral("+"));
  addBoardButton_->setToolTip(QStringLiteral("Create board"));
  addBoardButton_->setFixedSize(34, 34);

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
  contentLayout->setSpacing(8);

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
  body->setSpacing(10);
  auto* leftPane = new QWidget(contentWidget);
  auto* leftPaneLayout = new QVBoxLayout(leftPane);
  leftPaneLayout->setContentsMargins(0, 0, 0, 0);
  leftPaneLayout->setSpacing(8);

  boardNavFrame_ = new QWidget(leftPane);
  boardNavFrame_->setObjectName(QStringLiteral("boardNavFrame"));
  boardNavLayout_ = new QHBoxLayout(boardNavFrame_);
  boardNavLayout_->setContentsMargins(10, 8, 10, 8);
  boardNavLayout_->setSpacing(8);
  leftPaneLayout->addWidget(boardNavFrame_);

  gridScrollArea_ = new QScrollArea(leftPane);
  gridScrollArea_->setObjectName(QStringLiteral("gridScrollArea"));
  gridScrollArea_->setWidgetResizable(true);
  gridScrollArea_->setFrameShape(QFrame::NoFrame);
  gridScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  gridScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  gridHost_     = new QWidget(gridScrollArea_);
  gridHost_->setObjectName(QStringLiteral("gridHost"));
  gridHost_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  gridLayout_   = new QGridLayout(gridHost_);
  gridLayout_->setContentsMargins(0, 0, 0, 0);
  gridLayout_->setHorizontalSpacing(8);
  gridLayout_->setVerticalSpacing(8);
  gridScrollArea_->setWidget(gridHost_);
  leftPaneLayout->addWidget(gridScrollArea_, 1);

  pagerFrame_ = new QWidget(leftPane);
  pagerFrame_->setObjectName(QStringLiteral("pagerFrame"));
  pagerLayout_ = new QHBoxLayout(pagerFrame_);
  pagerLayout_->setContentsMargins(10, 8, 10, 8);
  pagerLayout_->setSpacing(6);
  leftPaneLayout->addWidget(pagerFrame_);

  libraryList_  = new QListWidget(contentWidget);
  libraryList_->setObjectName(QStringLiteral("libraryList"));
  libraryList_->setMinimumWidth(320);
  libraryList_->setSpacing(4);
  libraryList_->setContextMenuPolicy(Qt::CustomContextMenu);
  body->addWidget(leftPane, 2);
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

  connect(boardSelector_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
    if (index < 0) {
      return;
    }
    setSelectedCell(-1);
    if (onBoardSelected) onBoardSelected(boardSelector_->currentData().toString());
  });

  connect(addBoardButton_, &QToolButton::clicked, this, [this]() {
    openCreateBoardDialog();
  });

  connect(importButton_, &QPushButton::clicked, this, [this]() {
    if (onImportSound) onImportSound(selectedCellIndex_);
  });

  connect(youtubeButton_, &QPushButton::clicked, this, [this]() { openYouTubeDialog(); });

  connect(libraryList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    if (!item) return;
    if (selectedCellIndex_ >= 0)
      statusLabel_->setText(
        QStringLiteral("Ready to assign \"%1\" to the selected cell. Double-click to confirm.").arg(displayNameForItem(item))
      );
  });

  connect(libraryList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
    if (!item) return;
    const QString sid  = item->data(Qt::UserRole).toString();
    const QString text = displayNameForItem(item);
    if (selectedCellIndex_ >= 0 && onAssignSoundToCell) {
      onAssignSoundToCell(sid, selectedCellIndex_);
      statusLabel_->setText(QStringLiteral("Assigned \"%1\" to the selected cell.").arg(text));
      return;
    }
    if (onPlaySound) onPlaySound(sid);
  });

  connect(libraryList_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
    auto* item = libraryList_->itemAt(pos);
    if (!item) {
      return;
    }

    const QString soundId = item->data(Qt::UserRole).toString();
    if (soundId.isEmpty()) {
      return;
    }

    QMenu contextMenu;
    contextMenu.addAction(QStringLiteral("Rename..."), this, [this, soundId]() {
      showRenameDialog(soundId);
    });
    contextMenu.exec(libraryList_->viewport()->mapToGlobal(pos));
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
    "#boardNavFrame, #pagerFrame { background: %3; border: 1px solid %4; border-radius: 10px; }"
    "#pageLabel { color: %7; font-size: 12px; font-weight: 600; }"
    "#pageButton { min-width: 32px; padding: 5px 10px; font-weight: 600; }"

    "QCheckBox { color: %2; spacing: 6px; }"
    "QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid %8; border-radius: 3px; background: %15; }"
    "QCheckBox::indicator:checked { background: %16; border-color: %16; }"

    "QSpinBox { padding: 5px 8px; border: 1px solid %8; border-radius: 8px; background: %15; color: %2; }"

    "QSlider::groove:horizontal { height: 4px; background: %4; border-radius: 2px; }"
    "QSlider::handle:horizontal { width: 14px; height: 14px; background: %16; border-radius: 7px; margin: -5px 0; }"
    "QSlider::sub-page:horizontal { background: %16; border-radius: 2px; }"

    "#previewBar { background: %17; border: 1px solid %18; border-radius: 10px; }"
    "#previewLabel { color: %7; font-size: 12px; }"

    "#gridScrollArea { background: transparent; border: none; }"
    "#gridHost { background: transparent; }"
    "#cellEmojiButton { border: none; background: transparent; color: %2; padding: 0px; }"
    "#cellEmojiButton:hover { background: %10; border-radius: 6px; }"
    "#cellButton { border: none; background: transparent; color: %2; font-size: 12px; text-align: center; padding: 4px 6px 6px 6px; }"
    "#cellButton:hover { color: %2; }"
    "#cellDeleteButton { min-width: 18px; max-width: 18px; min-height: 18px; max-height: 18px; padding: 0px; border-radius: 9px; background: transparent; }"
    "#cellDeleteButton:hover { background: %10; }"

    "#libraryList { background: %3; border: 1px solid %4; border-radius: 10px; color: %2; }"
    "#libraryList::item { padding: 2px; border-radius: 8px; }"
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

  if (!rebuildingUi_) {
    rebuild();
  } else {
    setSelectedCell(selectedCellIndex_);
  }
}

// ---------------------------------------------------------------------------
// Remaining MainWindow methods
// ---------------------------------------------------------------------------

void MainWindow::setState(const AppState& state) {
  const QString previousActiveBoardId = state_.activeBoardId;
  state_ = state;
  if (previousActiveBoardId != state_.activeBoardId) {
    selectedCellIndex_ = -1;
  }
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
  for (int i = 0; i < cellCards_.size(); ++i) {
    if (!cellCards_[i]) continue;
    cellCards_[i]->setStyleSheet(QStringLiteral(
      "background: %1; border: 1px solid %2; border-radius: 10px;"
    ).arg(i == selectedCellIndex_ ? t.accentBg : t.cellBg,
          i == selectedCellIndex_ ? t.cellSelected : t.cellBorder));
  }
}

QString MainWindow::displayNameForItem(const QListWidgetItem* item) const {
  if (!item) {
    return QString();
  }

  const QString displayName = item->data(Qt::UserRole + 1).toString();
  return displayName.isEmpty() ? item->text() : displayName;
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
    const QString itemText = displayNameForItem(currentItem);
    onAssignSoundToCell(newSoundId, cellIndex);
    statusLabel_->setText(QStringLiteral("Assigned \"%1\" to the selected cell.").arg(itemText));
    return;
  }

  if (!soundId.isEmpty()) {
    if (onPlaySound) onPlaySound(soundId);
    return;
  }

  if (currentItem && onAssignSoundToCell) {
    onAssignSoundToCell(currentItem->data(Qt::UserRole).toString(), cellIndex);
    statusLabel_->setText(
      QStringLiteral("Assigned \"%1\" to the selected cell.").arg(displayNameForItem(currentItem))
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

void MainWindow::openCreateBoardDialog() {
  if (!onCreateBoard) {
    return;
  }

  bool accepted = false;
  const QString defaultName = QStringLiteral("Board %1").arg(state_.boards.size() + 1);
  const QString name = QInputDialog::getText(
    this,
    QStringLiteral("Create Board"),
    QStringLiteral("Board name:"),
    QLineEdit::Normal,
    defaultName,
    &accepted
  ).trimmed();

  if (!accepted) {
    return;
  }

  const QString boardName = name.isEmpty() ? defaultName : name;
  const BoardRecord* board = activeBoard();
  const int rows = board ? qMax(1, board->rows) : 3;
  const int cols = board ? qMax(1, board->cols) : 4;
  selectedCellIndex_ = -1;
  onCreateBoard(boardName, rows, cols);
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

  while (QLayoutItem* item = boardNavLayout_->takeAt(0)) {
    QWidget* widget = item->widget();
    if (widget && widget != boardSelector_ && widget != addBoardButton_) {
      item->widget()->deleteLater();
    }
    delete item;
  }
  while (QLayoutItem* item = pagerLayout_->takeAt(0)) {
    if (item->widget()) {
      item->widget()->deleteLater();
    }
    delete item;
  }
  pageButtons_.clear();
  const Theme t = darkMode_ ? darkTheme() : lightTheme();

  auto* pageLabel = new QLabel(QStringLiteral("Page"), boardNavFrame_);
  pageLabel->setObjectName(QStringLiteral("pageLabel"));
  boardNavLayout_->addWidget(pageLabel);
  boardNavLayout_->addWidget(boardSelector_, 1);
  boardNavLayout_->addWidget(addBoardButton_);
  boardNavLayout_->addStretch(1);

  auto* pagerLabel = new QLabel(QStringLiteral("Pages"), pagerFrame_);
  pagerLabel->setObjectName(QStringLiteral("pageLabel"));
  pagerLayout_->addWidget(pagerLabel);

  auto addPageJump = [this, &t](QHBoxLayout* layout, const QString& text, int targetIndex, bool enabled, bool active = false) {
    auto* button = new QPushButton(text);
    button->setObjectName(QStringLiteral("pageButton"));
    button->setEnabled(enabled);
    if (active) {
      button->setStyleSheet(QStringLiteral("background: %1; border-color: %2; font-weight: 700;")
        .arg(t.accentBg, t.cellSelected));
    }
    if (enabled) {
      connect(button, &QPushButton::clicked, this, [this, targetIndex]() {
        if (targetIndex >= 0 && targetIndex < boardSelector_->count()) {
          boardSelector_->setCurrentIndex(targetIndex);
        }
      });
    }
    layout->addWidget(button);
    pageButtons_.push_back(button);
  };

  const int currentBoardIndex = boardSelector_->currentIndex();
  addPageJump(pagerLayout_, QStringLiteral("<"), currentBoardIndex - 1, currentBoardIndex > 0);
  for (int i = 0; i < boardSelector_->count(); ++i) {
    addPageJump(pagerLayout_, QString::number(i + 1), i, true, i == currentBoardIndex);
  }
  addPageJump(pagerLayout_, QStringLiteral(">"), currentBoardIndex + 1, currentBoardIndex >= 0 && currentBoardIndex < boardSelector_->count() - 1);
  pagerLayout_->addStretch(1);

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

  cellCards_.clear();
  while (QLayoutItem* item = gridLayout_->takeAt(0)) {
    delete item->widget();
    delete item;
  }
  for (int i = 0; i < 50; ++i) {
    gridLayout_->setColumnStretch(i, 0);
    gridLayout_->setColumnMinimumWidth(i, 0);
    gridLayout_->setRowStretch(i, 0);
    gridLayout_->setRowMinimumHeight(i, 0);
  }
  gridHost_->setMinimumSize(0, 0);

  if (board) {
    const int safeCols = qMax(1, board->cols);
    const int safeRows = qMax(1, board->rows);
    const int density = qMax(safeRows, safeCols);
    const int gridSpacing = density >= 10 ? 4 : (density >= 7 ? 6 : 8);
    const int cellMargin = density >= 10 ? 4 : (density >= 7 ? 6 : 8);
    const int emojiSize = density >= 10 ? 22 : (density >= 7 ? 26 : 32);
    const int minCellWidth = density >= 10 ? 72 : (density >= 7 ? 88 : 116);
    const int minCellHeight = density >= 10 ? 58 : (density >= 7 ? 70 : 88);
    gridLayout_->setHorizontalSpacing(gridSpacing);
    gridLayout_->setVerticalSpacing(gridSpacing);
    for (int col = 0; col < safeCols; ++col) {
      gridLayout_->setColumnStretch(col, 1);
      gridLayout_->setColumnMinimumWidth(col, minCellWidth);
    }
    for (int row = 0; row < safeRows; ++row) {
      gridLayout_->setRowStretch(row, 1);
      gridLayout_->setRowMinimumHeight(row, minCellHeight);
    }
    gridHost_->setMinimumSize(
      (minCellWidth * safeCols) + (gridSpacing * qMax(0, safeCols - 1)),
      (minCellHeight * safeRows) + (gridSpacing * qMax(0, safeRows - 1))
    );

    for (int index = 0; index < board->cells.size(); ++index) {
      const int row = index / safeCols;
      const int col = index % safeCols;
      QString label = QStringLiteral("Empty cell");
      QString soundId;
      QString emoji = board->cells[index].icon;
      if (!board->cells[index].soundId.isEmpty()) {
        soundId = board->cells[index].soundId;
        for (const SoundRecord& sound : state_.library) {
          if (sound.soundId == soundId) {
            label = sound.displayName;
            break;
          }
        }
      }
      if (emoji.isEmpty()) {
        emoji = soundId.isEmpty() ? QStringLiteral("+") : emojiFromUtf8("\xF0\x9F\x94\x8A");
      }

      auto* cellWidget = new QWidget(gridHost_);
      cellWidget->setMinimumSize(minCellWidth, minCellHeight);
      cellWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      cellWidget->setStyleSheet(QStringLiteral(
        "background: %1; border: 1px solid %2; border-radius: 10px;"
      ).arg(t.cellBg, t.cellBorder));

      auto* cellLayout = new QVBoxLayout(cellWidget);
      cellLayout->setContentsMargins(cellMargin, cellMargin, cellMargin, cellMargin);
      cellLayout->setSpacing(density >= 7 ? 1 : 2);

      auto* headerLayout = new QHBoxLayout();
      headerLayout->setContentsMargins(0, 0, 0, 0);

      auto* emojiButton = new QPushButton(emoji, cellWidget);
      emojiButton->setObjectName(QStringLiteral("cellEmojiButton"));
      emojiButton->setCursor(Qt::PointingHandCursor);
      emojiButton->setToolTip(QStringLiteral("Change tile emoji"));
      emojiButton->setFixedSize(emojiSize + 8, emojiSize + 8);
      emojiButton->setStyleSheet(QStringLiteral(
        "#cellEmojiButton { font-size: %1px; }"
      ).arg(emojiSize));
      headerLayout->addStretch(1);
      headerLayout->addWidget(emojiButton, 0, Qt::AlignCenter);
      headerLayout->addStretch(1);

      auto* deleteButton = new QPushButton(QStringLiteral("x"), cellWidget);
      deleteButton->setObjectName(QStringLiteral("cellDeleteButton"));
      deleteButton->setToolTip(QStringLiteral("Clear tile"));
      deleteButton->setVisible(!soundId.isEmpty());
      headerLayout->addWidget(deleteButton, 0, Qt::AlignRight);
      cellLayout->addLayout(headerLayout);

      const QString buttonText = compactCellLabel(buildCellButtonLabel(board->cells[index], label), safeRows, safeCols);
      auto* button = new QPushButton(buttonText, cellWidget);
      button->setObjectName(QStringLiteral("cellButton"));
      button->setFlat(true);
      button->setCursor(Qt::PointingHandCursor);
      button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
      button->setMinimumHeight(qMax(24, minCellHeight - emojiSize - (cellMargin * 2) - 10));
      button->setToolTip(label);
      button->setContextMenuPolicy(Qt::CustomContextMenu);
      button->setStyleSheet(QStringLiteral(
        "#cellButton { background: transparent; border: none; color: %1; font-size: %2px; font-weight: 600; text-align: center; }"
      ).arg(t.textPrimary, QString::number(density >= 10 ? 10 : 12)));
      cellLayout->addWidget(button, 1);

      gridLayout_->addWidget(cellWidget, row, col);
      cellCards_.push_back(cellWidget);

      const QString currentSoundId = soundId;
      connect(emojiButton, &QPushButton::clicked, this, [this, index, emoji]() {
        EmojiPicker picker(emoji, this);
        if (picker.exec() == QDialog::Accepted && onCellEmojiChanged) {
          onCellEmojiChanged(index, picker.selectedEmoji());
        }
      });

      connect(button, &QWidget::customContextMenuRequested, this, [this, currentSoundId, index](const QPoint&) {
        QMenu contextMenu;
        contextMenu.addAction(QStringLiteral("Change Tile Emoji..."), this, [this, index]() {
          const BoardRecord* active = activeBoard();
          const QString currentIcon = active && index >= 0 && index < active->cells.size()
            ? active->cells[index].icon
            : QString();
          EmojiPicker picker(currentIcon, this);
          if (picker.exec() == QDialog::Accepted && onCellEmojiChanged) {
            onCellEmojiChanged(index, picker.selectedEmoji());
          }
        });
        if (!currentSoundId.isEmpty()) {
          contextMenu.addAction(QStringLiteral("Rename Sound..."), this, [this, currentSoundId]() {
            showRenameDialog(currentSoundId);
          });
          contextMenu.addAction(QStringLiteral("Clear Tile"), this, [this, index]() {
            if (onAssignSoundToCell) {
              onAssignSoundToCell(QString(), index);
              statusLabel_->setText(QStringLiteral("Cell cleared."));
            }
          });
        }
        contextMenu.exec(QCursor::pos());
      });

      connect(button, &QPushButton::clicked, this, [this, index, soundId]() {
        handleCellClick(index, soundId);
      });

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
    auto* item = new QListWidgetItem();
    item->setData(Qt::UserRole, sound.soundId);
    item->setData(Qt::UserRole + 1, sound.displayName);
    item->setSizeHint(QSize(0, 54));

    auto* rowWidget = new QWidget(libraryList_);
    rowWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(10, 6, 10, 6);
    rowLayout->setSpacing(8);

    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    auto* titleLabel = new QLabel(sound.displayName, rowWidget);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: 600; color: %1;").arg(t.textPrimary));
    titleLabel->setToolTip(sound.displayName);

    auto* metaLabel = new QLabel(libraryMetaLabel(sound), rowWidget);
    metaLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: %1;").arg(t.textMuted));

    auto* durationLabel = new QLabel(formatDurationMs(sound.durationMs), rowWidget);
    durationLabel->setMinimumWidth(46);
    durationLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    durationLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: %1;").arg(t.textMuted));

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(metaLabel);
    rowLayout->addWidget(durationLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    rowLayout->addLayout(textLayout, 1);

    libraryList_->addItem(item);
    libraryList_->setItemWidget(item, rowWidget);
  }

  if (selectedCellIndex_ >= cellCards_.size()) selectedCellIndex_ = -1;
  setSelectedCell(selectedCellIndex_);
}

}  // namespace rpsu
