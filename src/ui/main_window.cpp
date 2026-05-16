#include "src/ui/main_window.h"
#include "src/ui/emoji_picker.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
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
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QKeySequence>
#include <QShowEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMenu>
#include <QCloseEvent>
#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>
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
// Smooth-scroll filter: intercepts wheel events on a scrollable widget and
// animates the vertical scrollbar with an ease-out curve so scrolling glides
// to a stop rather than snapping. Subsequent wheel ticks chain onto the
// already-in-flight animation for a momentum feel.
// ---------------------------------------------------------------------------

class SmoothScrollFilter : public QObject {
 public:
  explicit SmoothScrollFilter(QAbstractScrollArea* target)
      : QObject(target), target_(target) {
    anim_ = new QPropertyAnimation(target->verticalScrollBar(), "value", this);
    anim_->setDuration(280);
    anim_->setEasingCurve(QEasingCurve::OutCubic);
    target->viewport()->installEventFilter(this);
  }

 protected:
  bool eventFilter(QObject* obj, QEvent* ev) override {
    if (ev->type() != QEvent::Wheel) return QObject::eventFilter(obj, ev);
    auto* we = static_cast<QWheelEvent*>(ev);
    auto* bar = target_->verticalScrollBar();
    // Use angleDelta().y() — positive = scroll up. 120 units per "notch".
    const int delta = we->angleDelta().y();
    if (delta == 0) return false;
    // ~3 lines per notch, scaled by viewport step.
    const int step = qMax(40, bar->singleStep() * 3);
    const int magnitude = (delta * step) / 120;
    const int startVal = (anim_->state() == QAbstractAnimation::Running)
                         ? anim_->endValue().toInt() : bar->value();
    const int target = qBound(bar->minimum(), startVal - magnitude, bar->maximum());
    anim_->stop();
    anim_->setStartValue(bar->value());
    anim_->setEndValue(target);
    anim_->start();
    return true;  // consume — we drive the scroll ourselves
  }

 private:
  QAbstractScrollArea* target_ = nullptr;
  QPropertyAnimation* anim_ = nullptr;
};

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

// Midnight slate dark mode — neutral near-black, no blue tint
static Theme darkTheme() {
  Theme t;
  t.windowBg            = QStringLiteral("#0e0f14");
  t.surfaceBg           = QStringLiteral("#17181f");
  t.surfaceBorder       = QStringLiteral("#2a2c37");
  t.textPrimary         = QStringLiteral("#e4e6ed");
  t.textMuted           = QStringLiteral("#6b7080");
  t.inputBg             = QStringLiteral("#1e1f27");
  t.inputBorder         = QStringLiteral("#333544");
  t.buttonBg            = QStringLiteral("#22242f");
  t.buttonBorder        = QStringLiteral("#33354a");
  t.buttonHover         = QStringLiteral("#2b2d3d");
  t.buttonDisabled      = QStringLiteral("#16171e");
  t.buttonDisabledText  = QStringLiteral("#44475a");
  t.accentBg            = QStringLiteral("#1e2d4a");
  t.accentBorder        = QStringLiteral("#2f4d80");
  t.tableAltRow         = QStringLiteral("#131419");
  t.tableGridline       = QStringLiteral("#22232d");
  t.headerBg            = QStringLiteral("#1a1b24");
  t.headerText          = QStringLiteral("#bbbec9");
  t.previewBg           = QStringLiteral("#111218");
  t.previewBorder       = QStringLiteral("#21222d");
  t.cellBorder          = QStringLiteral("#2c2e3c");
  t.cellBg              = QStringLiteral("#17181f");
  t.cellHover           = QStringLiteral("#21222d");
  t.cellSelected        = QStringLiteral("#3b82f6");
  t.statusBg            = QStringLiteral("#0c0d11");
  t.statusBorder        = QStringLiteral("#1e1f28");
  return t;
}

// ---------------------------------------------------------------------------
// Dark mode toggle icon helpers
// ---------------------------------------------------------------------------

// Crescent moon drawn with two overlapping circles (subtraction via clip).
static QIcon makeMoonIcon(int size) {
  QPixmap pm(size, size);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  const QColor fill(200, 210, 230);
  // Full circle
  p.setBrush(fill);
  p.setPen(Qt::NoPen);
  p.drawEllipse(pm.rect().adjusted(2, 2, -2, -2));
  // Clip out the top-right quarter to form a crescent
  p.setCompositionMode(QPainter::CompositionMode_Clear);
  p.drawEllipse(QRectF(size * 0.22, size * 0.0, size * 0.72, size * 0.72));
  p.end();
  return QIcon(pm);
}

// Simple sun: central circle + 8 short rays.
static QIcon makeSunIcon(int size) {
  QPixmap pm(size, size);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  const QColor fill(250, 200, 60);
  const double cx = size / 2.0, cy = size / 2.0;
  const double r  = size * 0.21;
  const double ro = size * 0.42;
  const double rw = size * 0.06;
  p.setBrush(fill);
  p.setPen(Qt::NoPen);
  // Rays
  for (int i = 0; i < 8; ++i) {
    const double angle = i * M_PI / 4.0;
    const double rx = cx + ro * std::cos(angle);
    const double ry = cy + ro * std::sin(angle);
    p.drawEllipse(QPointF(rx, ry), rw, rw);
  }
  // Core
  p.drawEllipse(QPointF(cx, cy), r, r);
  p.end();
  return QIcon(pm);
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

class LambdaEventFilter : public QObject {
 public:
  using Handler = std::function<bool(QObject*, QEvent*)>;
  explicit LambdaEventFilter(Handler h, QObject* parent = nullptr)
      : QObject(parent), handler_(std::move(h)) {}
  bool eventFilter(QObject* obj, QEvent* ev) override { return handler_(obj, ev); }
 private:
  Handler handler_;
};

class WidgetUpdatesBlocker {
 public:
  explicit WidgetUpdatesBlocker(QWidget* widget)
      : widget_(widget), wasEnabled_(widget ? widget->updatesEnabled() : false) {
    if (widget_) {
      widget_->setUpdatesEnabled(false);
    }
  }

  ~WidgetUpdatesBlocker() {
    if (widget_) {
      widget_->setUpdatesEnabled(wasEnabled_);
    }
  }

 private:
  QWidget* widget_ = nullptr;
  bool wasEnabled_ = false;
};

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
    resultsTable_->setColumnWidth(3, 110);
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
    using R = decltype(fn());
    // Reentrancy guard: if anything is already running, refuse the call.
    if (inFlight_.exchange(true)) {
      return R{};
    }
    setBusy(true);
    loadingTick_ = 0;

    // Capture the work in a packaged_task that swallows exceptions so a
    // failing yt-dlp / QProcess never tears down the future thread.
    auto safeWork = [fn = std::forward<Fn>(fn)]() mutable -> R {
      try { return fn(); }
      catch (...) { return R{}; }
    };

    auto future = std::async(std::launch::async, std::move(safeWork));

    // While the future runs, pump only paint / timer events — NEVER user
    // input. That prevents the user from clicking Preview/Download again
    // and starting a second concurrent operation (which previously could
    // crash the dashboard process).
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
      // Pump all events so the Cancel button stays responsive. Any other
      // operation entry point is gated by the inFlight_ atomic above and
      // will simply no-op while a future is running.
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    auto result = future.get();
    setBusy(false);
    inFlight_.store(false);
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
      actionLayout->setContentsMargins(4, 0, 4, 0);
      actionLayout->setSpacing(6);
      actionLayout->setAlignment(Qt::AlignCenter);

      // Hand-drawn play triangle (preview).
      auto makePlayIcon = [](int size, QColor fg) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(fg);
        // Equilateral-ish triangle, slightly inset and nudged right so it
        // looks visually centered (triangles have a heavier left edge).
        const double inset = size * 0.26;
        const double cx = size * 0.06;  // shift right
        QPolygonF tri({
          QPointF(inset + cx,            inset),
          QPointF(inset + cx,            size - inset),
          QPointF(size - inset + cx*0.5, size / 2.0)
        });
        p.drawPolygon(tri);
        return QIcon(pm);
      };

      // Hand-drawn download icon (down arrow + tray).
      auto makeDownloadIcon = [](int size, QColor fg) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(fg);
        pen.setWidthF(size * 0.10);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        // Vertical shaft
        const double cx = size / 2.0;
        const double topY = size * 0.20;
        const double tipY = size * 0.62;
        p.drawLine(QPointF(cx, topY), QPointF(cx, tipY));
        // Arrowhead
        const double head = size * 0.22;
        p.drawLine(QPointF(cx - head, tipY - head), QPointF(cx, tipY));
        p.drawLine(QPointF(cx + head, tipY - head), QPointF(cx, tipY));
        // Tray
        const double trayY = size * 0.80;
        const double trayInset = size * 0.18;
        p.drawLine(QPointF(trayInset, trayY), QPointF(trayInset, trayY + size * 0.08));
        p.drawLine(QPointF(size - trayInset, trayY), QPointF(size - trayInset, trayY + size * 0.08));
        p.drawLine(QPointF(trayInset, trayY + size * 0.08),
                   QPointF(size - trayInset, trayY + size * 0.08));
        return QIcon(pm);
      };

      auto* previewButton     = new QPushButton(actionHost);
      auto* downloadRowButton = new QPushButton(actionHost);
      previewButton->setCursor(Qt::PointingHandCursor);
      downloadRowButton->setCursor(Qt::PointingHandCursor);
      previewButton->setFixedSize(26, 26);
      downloadRowButton->setFixedSize(26, 26);
      previewButton->setIconSize(QSize(14, 14));
      downloadRowButton->setIconSize(QSize(14, 14));
      previewButton->setToolTip(QStringLiteral("Preview"));
      downloadRowButton->setToolTip(QStringLiteral("Download"));
      previewButton->setIcon(makePlayIcon(48, QColor(t.textPrimary)));
      // Download button uses a strong, mode-independent blue so the white
      // icon stays readable in both light and dark themes.
      downloadRowButton->setIcon(makeDownloadIcon(48, QColor(Qt::white)));
      previewButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; border-radius: 13px; padding: 0; }"
        "QPushButton:hover { background: %3; }")
        .arg(t.buttonBg, t.buttonBorder, t.buttonHover));
      downloadRowButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2563eb; border: 1px solid #1e40af; border-radius: 13px; padding: 0; }"
        "QPushButton:hover { background: #1d4ed8; }"));

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
      resultsTable_->setRowHeight(index, 40);
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
  // Hard reentrancy guard. Set the moment a future is launched, cleared
  // when it completes. Every entry point checks this before starting work.
  std::atomic<bool> inFlight_{false};
  static constexpr int kPageSize = 12;

 protected:
  void closeEvent(QCloseEvent* event) override {
    if (inFlight_.load()) {
      // Don't let the dialog be destroyed while a future references our
      // atomics — that would dangle the pointers we passed to yt-dlp glue.
      // Signal cancel and pump the event loop until the future finishes.
      cancelRequested_.store(true);
      statusLabel_->setText(QStringLiteral("Cancelling, please wait..."));
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
      while (inFlight_.load() && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
      }
      if (inFlight_.load()) {
        // Refuse to close — better than a crash. User can retry.
        event->ignore();
        return;
      }
    }
    QDialog::closeEvent(event);
  }
};

class HotkeyInputDialog : public QDialog {
 public:
  explicit HotkeyInputDialog(const QString& currentHotkey, QWidget* parent = nullptr)
      : QDialog(parent), key1_(currentHotkey) {
    setWindowTitle(QStringLiteral("Set Hotkey"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setFixedSize(420, 210);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* prompt = new QLabel(
      QStringLiteral("Click a box then press a key. Second box is optional."), this);
    layout->addWidget(prompt);

    auto* boxRow = new QHBoxLayout();
    boxRow->setSpacing(8);

    box1_ = new QPushButton(key1_.isEmpty() ? QStringLiteral("(click to set)") : key1_, this);
    box1_->setMinimumHeight(56);
    box1_->setFocusPolicy(Qt::NoFocus);
    boxRow->addWidget(box1_, 1);

    auto* plusLabel = new QLabel(QStringLiteral("+"), this);
    plusLabel->setAlignment(Qt::AlignCenter);
    plusLabel->setFixedWidth(24);
    boxRow->addWidget(plusLabel);

    box2_ = new QPushButton(QStringLiteral("(optional)"), this);
    box2_->setMinimumHeight(56);
    box2_->setFocusPolicy(Qt::NoFocus);
    boxRow->addWidget(box2_, 1);

    layout->addLayout(boxRow);

    auto* hintLabel = new QLabel(
      QStringLiteral("Hotkeys activate after restarting the TeamSpeak plugin."), this);
    hintLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: #888;"));
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    auto* btnRow = new QHBoxLayout();
    auto* clearBtn = new QPushButton(QStringLiteral("Clear"), this);
    clearBtn->setFixedWidth(80);
    clearBtn->setFocusPolicy(Qt::NoFocus);
    btnRow->addWidget(clearBtn);
    btnRow->addStretch();
    auto* buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnRow->addWidget(buttonBox);
    layout->addLayout(btnRow);

    connect(box1_, &QPushButton::clicked, this, [this]() {
      activeBox_ = 1;
      updateBoxStyles();
      setFocus();
    });
    connect(box2_, &QPushButton::clicked, this, [this]() {
      activeBox_ = 2;
      updateBoxStyles();
      setFocus();
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
      key1_.clear();
      key2_.clear();
      box1_->setText(QStringLiteral("(click to set)"));
      box2_->setText(QStringLiteral("(optional)"));
      activeBox_ = 0;
      updateBoxStyles();
    });

    updateBoxStyles();
  }

  QString hotkey() const {
    if (key1_.isEmpty()) return QString();
    if (key2_.isEmpty()) return key1_;
    return key1_ + QStringLiteral("+") + key2_;
  }

 protected:
  void showEvent(QShowEvent* event) override {
    QDialog::showEvent(event);
    setFocus();
  }

  void keyPressEvent(QKeyEvent* event) override {
    const int key = event->key();

    if (activeBox_ == 0) {
      if (key == Qt::Key_Escape) { reject(); return; }
      if (key == Qt::Key_Return || key == Qt::Key_Enter) { accept(); return; }
      return;
    }

    if (key == Qt::Key_unknown) return;
    if (key == Qt::Key_Escape) {
      activeBox_ = 0;
      updateBoxStyles();
      return;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
      accept();
      return;
    }

    QString captured;
    if (key == Qt::Key_Control || key == Qt::Key_Shift ||
        key == Qt::Key_Alt || key == Qt::Key_Meta) {
      captured = QKeySequence(key).toString(QKeySequence::NativeText);
    } else {
      captured = QKeySequence(static_cast<int>(event->modifiers()) | key)
                     .toString(QKeySequence::NativeText);
    }

    if (activeBox_ == 1) {
      key1_ = captured;
      box1_->setText(captured);
    } else {
      key2_ = captured;
      box2_->setText(captured);
    }
    activeBox_ = 0;
    updateBoxStyles();
  }

 private:
  void updateBoxStyles() {
    const QString activeStyle = QStringLiteral(
      "QPushButton { font-size: 16px; font-weight: bold; background: transparent; "
      "border: 2px solid #3b82f6; border-radius: 6px; padding: 8px; }");
    const QString normalStyle = QStringLiteral(
      "QPushButton { font-size: 16px; font-weight: bold; background: transparent; "
      "border: 1px solid #aaa; border-radius: 6px; padding: 8px; }");
    box1_->setStyleSheet(activeBox_ == 1 ? activeStyle : normalStyle);
    box2_->setStyleSheet(activeBox_ == 2 ? activeStyle : normalStyle);
  }

  int activeBox_ = 0;
  QString key1_;
  QString key2_;
  QPushButton* box1_ = nullptr;
  QPushButton* box2_ = nullptr;
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

  deleteBoardButton_ = new QToolButton(topBarFrame);
  deleteBoardButton_->setObjectName(QStringLiteral("deleteBoardButton"));
  deleteBoardButton_->setText(QStringLiteral("−"));  // minus sign
  deleteBoardButton_->setToolTip(QStringLiteral("Delete current board"));
  deleteBoardButton_->setFixedSize(34, 34);
  deleteBoardButton_->setCursor(Qt::PointingHandCursor);

  renameBoardButton_ = new QToolButton(topBarFrame);
  renameBoardButton_->setObjectName(QStringLiteral("renameBoardButton"));
  renameBoardButton_->setText(QStringLiteral("✎"));
  renameBoardButton_->setToolTip(QStringLiteral("Rename current board"));
  renameBoardButton_->setFixedSize(34, 34);
  renameBoardButton_->setCursor(Qt::PointingHandCursor);

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
  freesoundApiKey_->setFixedWidth(200);
  freesoundRow->addWidget(fsIconLabel);
  freesoundRow->addWidget(freesoundApiKey_);
  // Stretch goes to a spacer, NOT to the freesound field, so the dark mode
  // toggle stays visible and tight on the right edge.
  topBar->addWidget(freesoundWidget);
  topBar->addStretch(1);

  // Dark mode toggle (top-right)
  darkModeButton_ = new QToolButton(topBarFrame);
  darkModeButton_->setObjectName(QStringLiteral("darkModeButton"));
  darkModeButton_->setCheckable(true);
  darkModeButton_->setToolTip(QStringLiteral("Toggle dark / light mode"));
  darkModeButton_->setIcon(makeMoonIcon(24));
  darkModeButton_->setIconSize(QSize(24, 24));
  darkModeButton_->setFixedSize(38, 38);
  darkModeButton_->setCursor(Qt::PointingHandCursor);
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
  auto* previewOuterLayout = new QVBoxLayout(previewBar_);
  previewOuterLayout->setContentsMargins(14, 8, 14, 6);
  previewOuterLayout->setSpacing(4);
  auto* previewLayout = new QHBoxLayout();
  previewLayout->setSpacing(4);
  liveIndicator_ = new QLabel(previewBar_);
  liveIndicator_->setFixedSize(12, 12);
  liveIndicator_->setStyleSheet(QStringLiteral("background-color: #ef4444; border-radius: 6px;"));
  liveIndicator_->setVisible(false);
  previewLabel_ = new QLabel(QStringLiteral("Preview stopped"), previewBar_);
  previewLabel_->setObjectName(QStringLiteral("previewLabel"));
  // Hand-drawn stop icon (white rounded square inside a soft red circle).
  // Renders crisp at any DPI; no external assets needed.
  auto makeStopIcon = [](int size, bool enabled) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor bg     = enabled ? QColor("#ef4444") : QColor(160, 160, 160, 80);
    const QColor square = enabled ? QColor(Qt::white)  : QColor(255, 255, 255, 160);
    // Background disc
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(QRectF(0.5, 0.5, size - 1.0, size - 1.0));
    // Rounded stop square, ~38% of icon size, centered
    const double s = size * 0.38;
    const double x = (size - s) / 2.0;
    p.setBrush(square);
    QPainterPath path;
    path.addRoundedRect(QRectF(x, x, s, s), s * 0.18, s * 0.18);
    p.drawPath(path);
    return QIcon(pm);
  };

  // Pause/resume icon: two vertical bars (pause) or right-pointing triangle (resume)
  // inside a green circle. The 'showResume' flag selects which symbol to draw.
  auto makePauseIcon = [](int size, bool enabled, bool showResume) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor bg = enabled ? QColor("#22c55e") : QColor(160, 160, 160, 80);
    const QColor fg = enabled ? QColor(Qt::white)  : QColor(255, 255, 255, 160);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(QRectF(0.5, 0.5, size - 1.0, size - 1.0));
    p.setBrush(fg);
    if (showResume) {
      const double m = size * 0.28;
      const double off = size * 0.04;
      QPainterPath path;
      path.moveTo(m + off, m);
      path.lineTo(size - m + off, size / 2.0);
      path.lineTo(m + off, size - m);
      path.closeSubpath();
      p.drawPath(path);
    } else {
      const double barW = size * 0.13;
      const double barH = size * 0.38;
      const double barY = (size - barH) / 2.0;
      const double gap  = size * 0.08;
      const double totalW = 2.0 * barW + gap;
      const double startX = (size - totalW) / 2.0;
      const double r = barW * 0.3;
      p.drawRoundedRect(QRectF(startX, barY, barW, barH), r, r);
      p.drawRoundedRect(QRectF(startX + barW + gap, barY, barW, barH), r, r);
    }
    return QIcon(pm);
  };

  pausePreviewButton_ = new QPushButton(previewBar_);
  pausePreviewButton_->setObjectName(QStringLiteral("pausePreviewButton"));
  pausePreviewButton_->setEnabled(false);
  pausePreviewButton_->setFixedSize(32, 32);
  pausePreviewButton_->setIconSize(QSize(28, 28));
  pausePreviewButton_->setCursor(Qt::PointingHandCursor);
  pausePreviewButton_->setToolTip(QStringLiteral("Pause preview"));
  pausePreviewButton_->setFlat(true);
  pausePreviewButton_->setProperty("iconPause",    QVariant::fromValue(makePauseIcon(64, true,  false)));
  pausePreviewButton_->setProperty("iconResume",   QVariant::fromValue(makePauseIcon(64, true,  true)));
  pausePreviewButton_->setProperty("iconDisabled", QVariant::fromValue(makePauseIcon(64, false, false)));
  pausePreviewButton_->setIcon(pausePreviewButton_->property("iconDisabled").value<QIcon>());

  stopPreviewButton_ = new QPushButton(previewBar_);
  stopPreviewButton_->setObjectName(QStringLiteral("stopPreviewButton"));
  stopPreviewButton_->setEnabled(false);
  stopPreviewButton_->setFixedSize(32, 32);
  stopPreviewButton_->setIconSize(QSize(28, 28));
  stopPreviewButton_->setCursor(Qt::PointingHandCursor);
  stopPreviewButton_->setToolTip(QStringLiteral("Stop preview"));
  stopPreviewButton_->setFlat(true);
  // Store both states so setPreviewStatus() can swap without re-drawing.
  stopPreviewButton_->setProperty("iconEnabled",  QVariant::fromValue(makeStopIcon(64, true)));
  stopPreviewButton_->setProperty("iconDisabled", QVariant::fromValue(makeStopIcon(64, false)));
  stopPreviewButton_->setIcon(stopPreviewButton_->property("iconDisabled").value<QIcon>());
  speedButton_ = new QPushButton(QStringLiteral("1.0×"), previewBar_);
  speedButton_->setObjectName(QStringLiteral("speedButton"));
  speedButton_->setEnabled(false);
  speedButton_->setFixedHeight(26);
  speedButton_->setMinimumWidth(52);
  speedButton_->setCursor(Qt::PointingHandCursor);
  speedButton_->setToolTip(QStringLiteral("Playback speed — click to cycle"));
  speedButton_->setProperty("speedIndex", 0);

  previewLayout->addWidget(liveIndicator_);
  previewLayout->addSpacing(8);
  previewLayout->addWidget(previewLabel_, 1);
  previewLayout->addWidget(speedButton_);
  previewLayout->addWidget(pausePreviewButton_);
  previewLayout->addWidget(stopPreviewButton_);

  progressSlider_ = new QSlider(Qt::Horizontal, previewBar_);
  progressSlider_->setObjectName(QStringLiteral("progressSlider"));
  progressSlider_->setEnabled(false);
  progressSlider_->setRange(0, 0);
  progressSlider_->setValue(0);
  progressSlider_->setFixedHeight(16);
  progressSlider_->setCursor(Qt::PointingHandCursor);

  previewOuterLayout->addLayout(previewLayout);
  previewOuterLayout->addWidget(progressSlider_);
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

  // Library panel: toolbar + list
  auto* libraryPanel = new QWidget(contentWidget);
  libraryPanel->setMinimumWidth(200);
  auto* libraryPanelLayout = new QVBoxLayout(libraryPanel);
  libraryPanelLayout->setContentsMargins(0, 0, 0, 0);
  libraryPanelLayout->setSpacing(4);

  auto* libraryToolbar = new QWidget(libraryPanel);
  auto* libraryToolbarLayout = new QVBoxLayout(libraryToolbar);
  libraryToolbarLayout->setContentsMargins(0, 0, 0, 0);
  libraryToolbarLayout->setSpacing(4);

  librarySearch_ = new QLineEdit(libraryToolbar);
  librarySearch_->setObjectName(QStringLiteral("librarySearch"));
  librarySearch_->setPlaceholderText(QStringLiteral("Search sounds..."));
  librarySearch_->setClearButtonEnabled(true);

  librarySortCombo_ = new QComboBox(libraryToolbar);
  librarySortCombo_->setObjectName(QStringLiteral("librarySortCombo"));
  librarySortCombo_->addItem(QStringLiteral("A → Z"),         QStringLiteral("az"));
  librarySortCombo_->addItem(QStringLiteral("Z → A"),         QStringLiteral("za"));
  librarySortCombo_->addItem(QStringLiteral("Newest first"),  QStringLiteral("newest"));
  librarySortCombo_->addItem(QStringLiteral("Oldest first"),  QStringLiteral("oldest"));
  librarySortCombo_->addItem(QStringLiteral("Most played"),   QStringLiteral("mostplayed"));
  librarySortCombo_->addItem(QStringLiteral("Duration ↑"),    QStringLiteral("duration_asc"));
  librarySortCombo_->addItem(QStringLiteral("Duration ↓"),    QStringLiteral("duration_desc"));

  libraryToolbarLayout->addWidget(librarySearch_);
  libraryToolbarLayout->addWidget(librarySortCombo_);

  libraryList_  = new QListWidget(libraryPanel);
  libraryList_->setObjectName(QStringLiteral("libraryList"));
  libraryList_->setSpacing(4);
  libraryList_->setContextMenuPolicy(Qt::CustomContextMenu);
  // Smooth animated wheel-scrolling for the sound library.
  libraryList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  libraryList_->verticalScrollBar()->setSingleStep(20);
  new SmoothScrollFilter(libraryList_);

  libraryPanelLayout->addWidget(libraryToolbar);
  libraryPanelLayout->addWidget(libraryList_, 1);

  body->addWidget(leftPane, 2);
  body->addWidget(libraryPanel, 1);
  contentLayout->addLayout(body, 1);

  root->addWidget(contentWidget, 1);

  // ── Initial theme ────────────────────────────────────────────────────────
  applyTheme();

  // ── Connections ──────────────────────────────────────────────────────────
  connect(darkModeButton_, &QToolButton::toggled, this, [this](bool on) {
    darkMode_ = on;
    darkModeButton_->setIcon(on ? makeSunIcon(24) : makeMoonIcon(24));
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

  connect(deleteBoardButton_, &QToolButton::clicked, this, [this]() {
    if (state_.boards.size() <= 1) {
      QMessageBox::information(this, QStringLiteral("Delete board"),
        QStringLiteral("You can't delete the last remaining board."));
      return;
    }
    const BoardRecord* board = activeBoard();
    if (!board) return;
    const QString boardId = board->id;
    const QString boardName = board->name;
    const auto reply = QMessageBox::question(this, QStringLiteral("Delete board"),
      QStringLiteral("Delete board \"%1\"? This cannot be undone.").arg(boardName),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes && onDeleteBoard) {
      onDeleteBoard(boardId);
    }
  });

  connect(renameBoardButton_, &QToolButton::clicked, this, [this]() {
    const BoardRecord* board = activeBoard();
    if (!board) return;
    bool ok = false;
    const QString newName = QInputDialog::getText(
      this, QStringLiteral("Rename board"), QStringLiteral("New board name:"),
      QLineEdit::Normal, board->name, &ok).trimmed();
    if (ok && !newName.isEmpty() && newName != board->name && onRenameBoard) {
      onRenameBoard(board->id, newName);
    }
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

  connect(librarySearch_, &QLineEdit::textChanged, this, [this]() { rebuild(); });
  connect(librarySortCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { rebuild(); });

  connect(libraryList_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
    auto* item = libraryList_->itemAt(pos);
    if (!item) {
      return;
    }

    const QString soundId = item->data(Qt::UserRole).toString();
    if (soundId.isEmpty()) {
      return;
    }

    // Look up display name for the confirmation prompt.
    QString displayName;
    for (const SoundRecord& s : state_.library) {
      if (s.soundId == soundId) { displayName = s.displayName; break; }
    }

    QMenu contextMenu;
    contextMenu.addAction(QStringLiteral("Rename..."), this, [this, soundId]() {
      showRenameDialog(soundId);
    });
    contextMenu.addSeparator();
    contextMenu.addAction(QStringLiteral("Delete sound"), this, [this, soundId, displayName]() {
      const auto reply = QMessageBox::question(this, QStringLiteral("Delete sound"),
        QStringLiteral("Delete \"%1\"? The audio file will be removed. This cannot be undone.")
          .arg(displayName.isEmpty() ? soundId : displayName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (reply == QMessageBox::Yes && onDeleteSound) {
        onDeleteSound(soundId);
      }
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
  connect(speedButton_, &QPushButton::clicked, this, [this]() {
    static const double kSpeeds[] = {1.0, 1.25, 1.5, 2.0};
    static const char* kLabels[]  = {"1.0×", "1.25×", "1.5×", "2.0×"};
    const int idx = (speedButton_->property("speedIndex").toInt() + 1) % 4;
    speedButton_->setProperty("speedIndex", idx);
    speedButton_->setText(QLatin1String(kLabels[idx]));
    if (onSpeedChanged) onSpeedChanged(kSpeeds[idx]);
  });
  connect(progressSlider_, &QSlider::sliderPressed, this, [this]() {
    sliderDragging_ = true;
  });
  connect(progressSlider_, &QSlider::sliderReleased, this, [this]() {
    sliderDragging_ = false;
    if (onSeekPreview) onSeekPreview(progressSlider_->value());
  });
  connect(pausePreviewButton_, &QPushButton::clicked, this, [this]() {
    if (onPausePreview) onPausePreview();
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
    "#speedButton { padding: 2px 8px; font-size: 12px; font-weight: 600; }"

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
    "#stopPreviewButton { background: transparent; border: none; padding: 0px; }"
    "#stopPreviewButton:hover:enabled { background: rgba(239,68,68,0.12); border-radius: 16px; }"

    "#gridScrollArea { background: transparent; border: none; }"
    "#gridHost { background: transparent; }"
    "#cellEmojiButton { border: none; background: transparent; color: %2; padding: 0px; }"
    "#cellEmojiButton:hover { background: %10; border-radius: 6px; }"
    "#cellButton { border: none; background: transparent; color: %2; font-size: 12px; text-align: center; padding: 4px 6px 6px 6px; }"
    "#cellButton:hover { color: %2; }"
    "#cellDeleteButton { min-width: 18px; max-width: 18px; min-height: 18px; max-height: 18px; padding: 0px; border-radius: 9px; background: transparent; }"
    "#cellDeleteButton:hover { background: %10; }"

    "#librarySearch { background: %15; border: 1px solid %4; border-radius: 6px; color: %2; padding: 4px 8px; }"
    "#librarySortCombo { background: %9; border: 1px solid %8; border-radius: 6px; color: %2; padding: 2px 4px; }"
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

void MainWindow::setPreviewStatus(const QString& title, int durationMs, bool playing, bool paused) {
  if (!playing || title.trimmed().isEmpty()) {
    previewLabel_->setText(QStringLiteral("Preview stopped"));
    speedButton_->setEnabled(false);
    pausePreviewButton_->setEnabled(false);
    pausePreviewButton_->setIcon(pausePreviewButton_->property("iconDisabled").value<QIcon>());
    pausePreviewButton_->setToolTip(QStringLiteral("Pause preview"));
    stopPreviewButton_->setEnabled(false);
    stopPreviewButton_->setIcon(stopPreviewButton_->property("iconDisabled").value<QIcon>());
    progressSlider_->setEnabled(false);
    { QSignalBlocker b(progressSlider_); progressSlider_->setRange(0, 0); progressSlider_->setValue(0); }
    liveIndicator_->setVisible(false);
    return;
  }
  if (paused) {
    previewLabel_->setText(
      QStringLiteral("Paused: %1  [%2]").arg(title, formatDurationMs(durationMs))
    );
    pausePreviewButton_->setIcon(pausePreviewButton_->property("iconResume").value<QIcon>());
    pausePreviewButton_->setToolTip(QStringLiteral("Resume preview"));
  } else {
    previewLabel_->setText(
      QStringLiteral("Playing: %1  [%2]").arg(title, formatDurationMs(durationMs))
    );
    pausePreviewButton_->setIcon(pausePreviewButton_->property("iconPause").value<QIcon>());
    pausePreviewButton_->setToolTip(QStringLiteral("Pause preview"));
  }
  speedButton_->setEnabled(true);
  pausePreviewButton_->setEnabled(true);
  stopPreviewButton_->setIcon(stopPreviewButton_->property("iconEnabled").value<QIcon>());
  stopPreviewButton_->setEnabled(true);
  if (durationMs > 0) {
    QSignalBlocker b(progressSlider_);
    progressSlider_->setRange(0, durationMs);
    progressSlider_->setEnabled(true);
  }
  liveIndicator_->setVisible(!paused);
}

void MainWindow::updatePreviewProgress(int posMs) {
  if (!sliderDragging_) {
    QSignalBlocker b(progressSlider_);
    progressSlider_->setValue(posMs);
  }
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
  const WidgetUpdatesBlocker gridUpdates(gridHost_);
  const WidgetUpdatesBlocker libraryUpdates(libraryList_);
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
    if (widget && widget != boardSelector_ && widget != addBoardButton_ &&
        widget != deleteBoardButton_ && widget != renameBoardButton_) {
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
  QHash<QString, QString> soundNamesById;
  soundNamesById.reserve(state_.library.size());
  for (const SoundRecord& sound : state_.library) {
    soundNamesById.insert(sound.soundId, sound.displayName);
  }

  auto* pageLabel = new QLabel(QStringLiteral("Page"), boardNavFrame_);
  pageLabel->setObjectName(QStringLiteral("pageLabel"));
  boardNavLayout_->addWidget(pageLabel);
  boardNavLayout_->addWidget(boardSelector_, 1);
  boardNavLayout_->addWidget(renameBoardButton_);
  boardNavLayout_->addWidget(addBoardButton_);
  boardNavLayout_->addWidget(deleteBoardButton_);
  // Disable delete when only one board remains.
  deleteBoardButton_->setEnabled(state_.boards.size() > 1);
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
    cellCards_.reserve(board->cells.size());
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
        label = soundNamesById.value(soundId, label);
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

      auto* button = new QLabel(buildCellButtonLabel(board->cells[index], label), cellWidget);
      button->setObjectName(QStringLiteral("cellButton"));
      button->setCursor(Qt::PointingHandCursor);
      button->setWordWrap(true);
      button->setAlignment(Qt::AlignCenter);
      button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      button->setMinimumHeight(qMax(24, minCellHeight - emojiSize - (cellMargin * 2) - 10));
      button->setToolTip(label);
      button->setContextMenuPolicy(Qt::CustomContextMenu);
      button->setStyleSheet(QStringLiteral(
        "#cellButton { background: transparent; color: %1; font-size: %2px; font-weight: 600; }"
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

      auto showContextMenu = [this, currentSoundId, index]() {
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
        contextMenu.addAction(QStringLiteral("Set Hotkey..."), this, [this, index]() {
          const BoardRecord* active = activeBoard();
          const QString currentHotkey = active && index >= 0 && index < active->cells.size()
            ? active->cells[index].hotkey
            : QString();
          HotkeyInputDialog dlg(currentHotkey, this);
          if (dlg.exec() == QDialog::Accepted && onCellHotkeyChanged) {
            onCellHotkeyChanged(index, dlg.hotkey());
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
      };

      auto* buttonFilter = new LambdaEventFilter(
        [this, index, soundId, showContextMenu](QObject*, QEvent* ev) -> bool {
          if (ev->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::LeftButton) {
              handleCellClick(index, soundId);
              return true;
            }
            if (me->button() == Qt::RightButton) {
              showContextMenu();
              return true;
            }
          }
          return false;
        }, button);
      button->installEventFilter(buttonFilter);

      connect(deleteButton, &QPushButton::clicked, this, [this, index]() {
        if (onAssignSoundToCell) {
          onAssignSoundToCell(QString(), index);
          statusLabel_->setText(QStringLiteral("Cell cleared."));
        }
      });
    }
  }

  // Build filtered + sorted view of the library
  struct LibraryViewItem {
    const SoundRecord* sound = nullptr;
    QString displayNameLower;
  };

  QVector<LibraryViewItem> libraryView;
  libraryView.reserve(state_.library.size());
  {
    const QString filter = librarySearch_ ? librarySearch_->text().trimmed().toLower() : QString{};
    for (const SoundRecord& s : state_.library) {
      const QString displayNameLower = s.displayName.toLower();
      if (!filter.isEmpty() && !displayNameLower.contains(filter))
        continue;
      libraryView.push_back({&s, displayNameLower});
    }
    const QString sortKey = librarySortCombo_ ? librarySortCombo_->currentData().toString() : QStringLiteral("az");
    if (sortKey == QLatin1String("az")) {
      std::stable_sort(libraryView.begin(), libraryView.end(), [](const LibraryViewItem& a, const LibraryViewItem& b) {
        return a.displayNameLower < b.displayNameLower;
      });
    } else if (sortKey == QLatin1String("za")) {
      std::stable_sort(libraryView.begin(), libraryView.end(), [](const LibraryViewItem& a, const LibraryViewItem& b) {
        return a.displayNameLower > b.displayNameLower;
      });
    } else if (sortKey == QLatin1String("newest")) {
      std::stable_sort(libraryView.begin(), libraryView.end(), [](const LibraryViewItem& a, const LibraryViewItem& b) {
        return a.sound->createdAt > b.sound->createdAt;
      });
    } else if (sortKey == QLatin1String("oldest")) {
      std::stable_sort(libraryView.begin(), libraryView.end(), [](const LibraryViewItem& a, const LibraryViewItem& b) {
        return a.sound->createdAt < b.sound->createdAt;
      });
    } else if (sortKey == QLatin1String("mostplayed")) {
      std::stable_sort(libraryView.begin(), libraryView.end(), [](const LibraryViewItem& a, const LibraryViewItem& b) {
        return a.sound->playCount > b.sound->playCount;
      });
    } else if (sortKey == QLatin1String("duration_asc")) {
      std::stable_sort(libraryView.begin(), libraryView.end(), [](const LibraryViewItem& a, const LibraryViewItem& b) {
        return a.sound->durationMs < b.sound->durationMs;
      });
    } else if (sortKey == QLatin1String("duration_desc")) {
      std::stable_sort(libraryView.begin(), libraryView.end(), [](const LibraryViewItem& a, const LibraryViewItem& b) {
        return a.sound->durationMs > b.sound->durationMs;
      });
    }
  }

  libraryList_->clear();
  for (const LibraryViewItem& viewItem : libraryView) {
    const SoundRecord& sound = *viewItem.sound;
    auto* item = new QListWidgetItem();
    item->setData(Qt::UserRole, sound.soundId);
    item->setData(Qt::UserRole + 1, sound.displayName);
    item->setSizeHint(QSize(0, 54));

    auto* rowWidget = new QWidget(libraryList_);
    rowWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* rowLayout = new QVBoxLayout(rowWidget);
    rowLayout->setContentsMargins(10, 5, 10, 5);
    rowLayout->setSpacing(2);

    auto* titleLabel = new QLabel(sound.displayName, rowWidget);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 12px; color: %1;").arg(t.textPrimary));
    titleLabel->setToolTip(sound.displayName);
    titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    auto* metaRow = new QHBoxLayout();
    metaRow->setContentsMargins(0, 0, 0, 0);
    metaRow->setSpacing(6);

    auto* metaLabel = new QLabel(libraryMetaLabel(sound), rowWidget);
    metaLabel->setStyleSheet(QStringLiteral("font-size: 10px; color: %1;").arg(t.textMuted));

    auto* durationLabel = new QLabel(formatDurationMs(sound.durationMs), rowWidget);
    durationLabel->setStyleSheet(QStringLiteral("font-size: 10px; font-weight: 700; color: %1;").arg(t.textMuted));

    metaRow->addWidget(metaLabel);
    metaRow->addStretch(1);
    metaRow->addWidget(durationLabel);

    rowLayout->addWidget(titleLabel);
    rowLayout->addLayout(metaRow);

    libraryList_->addItem(item);
    libraryList_->setItemWidget(item, rowWidget);
  }

  if (selectedCellIndex_ >= cellCards_.size()) selectedCellIndex_ = -1;
  setSelectedCell(selectedCellIndex_);
}

}  // namespace rpsu
