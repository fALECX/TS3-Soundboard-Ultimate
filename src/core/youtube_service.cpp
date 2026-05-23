#include "src/core/youtube_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#ifdef _WIN32
#include <windows.h>
#endif

#include "src/core/models.h"

namespace rpsu {

#ifdef _WIN32
void hideProcessWindow(QProcess& process) {
  process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
    args->flags |= CREATE_NO_WINDOW;
  });
}
#else
void hideProcessWindow(QProcess&) {}
#endif

namespace {

constexpr int kPollIntervalMs = 200;
constexpr int kTailBufSize = 4096;
constexpr int kSearchTimeoutMs = 20000;
constexpr int kStartTimeoutMs = 10000;
// Stall threshold for the full download: if yt-dlp/ffmpeg produce no new
// output for this long we assume it is hung and kill it. We deliberately
// do NOT cap total wall-clock time — long videos (1h+) legitimately need
// several minutes to download + convert and a fixed cap killed those.
constexpr int kDownloadStallTimeoutMs = 120000;
constexpr int kPreviewTimeoutMs = 60000;

QString firstLine(const QString& text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return QString();
  }

  const int newline = trimmed.indexOf(QLatin1Char('\n'));
  return newline >= 0 ? trimmed.left(newline) : trimmed;
}

QString runWhereCommand(const QString& executable) {
  QProcess process;
  hideProcessWindow(process);
  process.start(QStringLiteral("where"), { executable });
  if (!process.waitForFinished(5000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    return QString();
  }

  const QStringList lines = QString::fromLocal8Bit(process.readAllStandardOutput())
                              .split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
  return lines.isEmpty() ? QString() : lines.front().trimmed();
}

QString findWingetTool(const QString& executableName) {
  const QString packagesRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
                                 .filePath(QStringLiteral("AppData/Local/Microsoft/WinGet/Packages"));
  if (!QFileInfo::exists(packagesRoot)) {
    return QString();
  }
  QDirIterator it(packagesRoot, { executableName }, QDir::Files, QDirIterator::Subdirectories);
  return it.hasNext() ? it.next() : QString();
}

#ifdef _WIN32
QString moduleDirectory() {
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleDirectory), &module)) {
    return {};
  }

  wchar_t buffer[MAX_PATH] = {0};
  const DWORD length = GetModuleFileNameW(module, buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) {
    return {};
  }

  return QFileInfo(QString::fromWCharArray(buffer)).absolutePath();
}
#else
QString moduleDirectory() {
  return QCoreApplication::applicationDirPath();
}
#endif

QString resolveToolPath(
  const QString& whereQuery,
  const QStringList& bundledCandidates,
  const QString& wingetQuery,
  const QString& missingError,
  QString* errorMessage,
  QString& cachedPath,
  bool& pathCached
) {
  if (pathCached) {
    if (errorMessage && cachedPath.isEmpty()) {
      *errorMessage = missingError;
    }
    return cachedPath;
  }

  for (const QString& bundledPath : bundledCandidates) {
    if (QFileInfo::exists(bundledPath)) {
      cachedPath = bundledPath;
      pathCached = true;
      return bundledPath;
    }
  }

  const QString pathTool = runWhereCommand(whereQuery);
  if (!pathTool.isEmpty()) {
    cachedPath = pathTool;
    pathCached = true;
    return pathTool;
  }

  if (!wingetQuery.isEmpty()) {
    const QString wingetPath = findWingetTool(wingetQuery);
    if (!wingetPath.isEmpty()) {
      cachedPath = wingetPath;
      pathCached = true;
      return wingetPath;
    }
  }

  if (errorMessage) {
    *errorMessage = missingError;
  }
  pathCached = true;
  return QString();
}

}  // namespace

QString YouTubeService::resolveYtDlpPath(QString* errorMessage) const {
  return resolveToolPath(
    QStringLiteral("yt-dlp"),
    {
      QDir(moduleDirectory()).filePath(QStringLiteral("yt-dlp.exe")),
      QDir(moduleDirectory()).filePath(QStringLiteral("rp_soundboard_ultimate/yt-dlp.exe"))
    },
    QString(),
    QStringLiteral("yt-dlp.exe is missing from the packaged dashboard files."),
    errorMessage,
    cachedYtDlpPath_,
    ytDlpPathCached_
  );
}

QString YouTubeService::resolveFfmpegPath(QString* errorMessage) const {
  return resolveToolPath(
    QStringLiteral("ffmpeg"),
    {
      QDir(moduleDirectory()).filePath(QStringLiteral("ffmpeg.exe")),
      QDir(moduleDirectory()).filePath(QStringLiteral("rp_soundboard_ultimate/ffmpeg.exe"))
    },
    QStringLiteral("ffmpeg.exe"),
    QStringLiteral("ffmpeg.exe was not found. Install FFmpeg or add it to PATH."),
    errorMessage,
    cachedFfmpegPath_,
    ffmpegPathCached_
  );
}

QVector<YouTubeSearchResult> YouTubeService::search(const QString& query, int limit, QString* errorMessage) const {
  QVector<YouTubeSearchResult> results;
  QString ytDlpError;
  const QString ytDlpPath = resolveYtDlpPath(&ytDlpError);
  if (ytDlpPath.isEmpty()) {
    if (errorMessage) {
      *errorMessage = ytDlpError;
    }
    return results;
  }

  QProcess process;
  hideProcessWindow(process);
  process.start(
    ytDlpPath,
    {
      QStringLiteral("ytsearch%1:%2").arg(QString::number(qMax(1, limit)), query.trimmed()),
      QStringLiteral("--dump-json"),
      QStringLiteral("--flat-playlist"),
      QStringLiteral("--no-warnings")
    }
  );

  if (!process.waitForFinished(kSearchTimeoutMs)) {
    process.kill();
    if (errorMessage) {
      *errorMessage = QStringLiteral("YouTube search timed out.");
    }
    return results;
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      *errorMessage = firstLine(QString::fromLocal8Bit(process.readAllStandardError()));
      if (errorMessage->isEmpty()) {
        *errorMessage = QStringLiteral("YouTube search failed.");
      }
    }
    return results;
  }

  const QStringList lines = QString::fromUtf8(process.readAllStandardOutput())
                              .split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
  for (const QString& line : lines) {
    const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
    if (!doc.isObject()) {
      continue;
    }

    const QJsonObject object = doc.object();
    YouTubeSearchResult result;
    result.id = object.value(QStringLiteral("id")).toString();
    result.title = object.value(QStringLiteral("title")).toString();
    result.url = object.value(QStringLiteral("url")).toString();
    result.channel = object.value(QStringLiteral("channel")).toString(object.value(QStringLiteral("uploader")).toString());
    result.durationSeconds = object.value(QStringLiteral("duration")).toInt(0);
    if (result.url.isEmpty() && !result.id.isEmpty()) {
      result.url = QStringLiteral("https://www.youtube.com/watch?v=%1").arg(result.id);
    }
    if (!result.title.isEmpty() && !result.url.isEmpty()) {
      results.push_back(result);
    }
  }

  if (results.isEmpty() && errorMessage) {
    *errorMessage = QStringLiteral("No YouTube results found.");
  }
  return results;
}

bool YouTubeService::downloadAudio(
  const YouTubeSearchResult& result,
  const QString& soundsDir,
  const QStringList& existingNames,
  QString* importedFilename,
  QString* errorMessage,
  std::atomic<bool>* cancelFlag,
  std::atomic<int>* progressPct
) const {
  QString ytDlpError;
  const QString ytDlpPath = resolveYtDlpPath(&ytDlpError);
  if (ytDlpPath.isEmpty()) {
    if (errorMessage) {
      *errorMessage = ytDlpError;
    }
    return false;
  }

  QString ffmpegError;
  const QString ffmpegPath = resolveFfmpegPath(&ffmpegError);
  if (ffmpegPath.isEmpty()) {
    if (errorMessage) {
      *errorMessage = ffmpegError;
    }
    return false;
  }

  const QString safeBase = sanitizeFilenameBase(result.title);
  const QString desiredFilename = ensureUniqueFilename(existingNames, safeBase + QStringLiteral(".wav"));
  const QString outputTemplate = QDir(soundsDir).filePath(QFileInfo(desiredFilename).completeBaseName() + QStringLiteral(".%(ext)s"));

  QProcess process;
  hideProcessWindow(process);
  process.setWorkingDirectory(soundsDir);
  // Merge stderr into stdout so we drain a single pipe; yt-dlp's stderr can
  // otherwise fill and block the child process on Windows (silent hang).
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(
    ytDlpPath,
    {
      result.url,
      QStringLiteral("--extract-audio"),
      QStringLiteral("--audio-format"),
      QStringLiteral("wav"),
      QStringLiteral("--audio-quality"),
      QStringLiteral("0"),
      QStringLiteral("--ffmpeg-location"),
      QFileInfo(ffmpegPath).absolutePath(),
      QStringLiteral("--no-playlist"),
      QStringLiteral("--no-warnings"),
      QStringLiteral("--newline"),
      QStringLiteral("-o"),
      outputTemplate
    }
  );

  if (!process.waitForStarted(kStartTimeoutMs)) {
    if (errorMessage) *errorMessage = QStringLiteral("yt-dlp failed to start.");
    return false;
  }

  static const QRegularExpression kProgressRe(
    QStringLiteral(R"(\[download\]\s+(\d+(?:\.\d+)?)%)")
  );

  // Tail buffer: keep only the last ~4 KiB of output so regex stays O(1)
  // and memory doesn't grow unbounded over long downloads.
  QString tail;
  QString lastErrLine;
  int msSinceLastProgress = 0;
  bool sawAnyOutput = false;

  auto drain = [&]() -> bool {
    const QByteArray chunk = process.readAllStandardOutput();
    if (chunk.isEmpty()) return false;
    tail.append(QString::fromLocal8Bit(chunk));
    if (tail.size() > kTailBufSize) tail = tail.right(kTailBufSize);
    // Track last non-empty line as a candidate error message.
    const QStringList lines = QString::fromLocal8Bit(chunk).split(
      QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    for (const QString& l : lines) {
      const QString t = l.trimmed();
      if (!t.isEmpty() && !t.startsWith(QLatin1Char('['))) lastErrLine = t;
    }
    return true;
  };

  while (true) {
    if (process.waitForFinished(kPollIntervalMs)) { drain(); break; }
    const bool gotOutput = drain();
    if (gotOutput) {
      sawAnyOutput = true;
      msSinceLastProgress = 0;
    } else {
      msSinceLastProgress += kPollIntervalMs;
    }

    if (cancelFlag && cancelFlag->load()) {
      process.kill();
      process.waitForFinished(3000);
      if (errorMessage) *errorMessage = QStringLiteral("Download cancelled.");
      return false;
    }

    // Only stall-timeout, no wall-clock cap: a 1-hour video legitimately
    // takes several minutes to download + convert. We only abort if yt-dlp
    // produced no new output for kDownloadStallTimeoutMs (hung child).
    if (msSinceLastProgress >= kDownloadStallTimeoutMs) {
      process.kill();
      process.waitForFinished(3000);
      if (errorMessage) {
        *errorMessage = sawAnyOutput
          ? QStringLiteral("YouTube download stalled (no progress for %1s).").arg(kDownloadStallTimeoutMs / 1000)
          : QStringLiteral("yt-dlp produced no output.");
      }
      return false;
    }

    if (progressPct) {
      if (tail.contains(QStringLiteral("[ExtractAudio]"))) {
        progressPct->store(100);
      } else {
        QRegularExpressionMatchIterator it = kProgressRe.globalMatch(tail);
        QRegularExpressionMatch last;
        while (it.hasNext()) last = it.next();
        if (last.hasMatch()) {
          progressPct->store(qBound(0, static_cast<int>(last.captured(1).toDouble()), 99));
        }
      }
    }
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      *errorMessage = !lastErrLine.isEmpty() ? lastErrLine : firstLine(tail);
      if (errorMessage->isEmpty()) {
        *errorMessage = QStringLiteral("YouTube download failed.");
      }
    }
    return false;
  }

  const QString finalPath = QDir(soundsDir).filePath(desiredFilename);
  if (!QFileInfo::exists(finalPath)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("The YouTube download did not produce a WAV file.");
    }
    return false;
  }

  if (importedFilename) {
    *importedFilename = desiredFilename;
  }
  return true;
}

bool YouTubeService::downloadPreviewAudio(
  const YouTubeSearchResult& result,
  const QString& previewDir,
  QString* previewPath,
  QString* errorMessage
) const {
  QString ytDlpError;
  const QString ytDlpPath = resolveYtDlpPath(&ytDlpError);
  if (ytDlpPath.isEmpty()) {
    if (errorMessage) {
      *errorMessage = ytDlpError;
    }
    return false;
  }

  QString ffmpegError;
  const QString ffmpegPath = resolveFfmpegPath(&ffmpegError);
  if (ffmpegPath.isEmpty()) {
    if (errorMessage) {
      *errorMessage = ffmpegError;
    }
    return false;
  }

  QDir dir(previewDir);
  if (!dir.exists() && !QDir().mkpath(previewDir)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Could not create the preview cache directory.");
    }
    return false;
  }

  const QString desiredFilename = sanitizeFilenameBase(result.title + QStringLiteral("-%1-preview").arg(result.id)) + QStringLiteral(".wav");
  const QString absolutePreviewPath = dir.filePath(desiredFilename);
  QFile::remove(absolutePreviewPath);

  QProcess process;
  hideProcessWindow(process);
  process.setWorkingDirectory(previewDir);
  // Merge channels so we drain a single pipe and avoid stderr deadlocks.
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(
    ytDlpPath,
    {
      result.url,
      QStringLiteral("--extract-audio"),
      QStringLiteral("--audio-format"),
      QStringLiteral("wav"),
      QStringLiteral("--audio-quality"),
      QStringLiteral("0"),
      QStringLiteral("--download-sections"),
      QStringLiteral("*0-20"),
      QStringLiteral("--force-keyframes-at-cuts"),
      QStringLiteral("--ffmpeg-location"),
      QFileInfo(ffmpegPath).absolutePath(),
      QStringLiteral("--no-playlist"),
      QStringLiteral("--no-warnings"),
      QStringLiteral("-o"),
      absolutePreviewPath
    }
  );

  if (!process.waitForStarted(kStartTimeoutMs)) {
    if (errorMessage) *errorMessage = QStringLiteral("yt-dlp failed to start.");
    return false;
  }

  // Drain in a polling loop so the output pipe never fills up.
  QString tail;
  int elapsedMs = 0;

  auto drain = [&]() {
    tail.append(QString::fromLocal8Bit(process.readAllStandardOutput()));
    if (tail.size() > kTailBufSize) tail = tail.right(kTailBufSize);
  };

  while (true) {
    if (process.waitForFinished(kPollIntervalMs)) { drain(); break; }
    drain();
    elapsedMs += kPollIntervalMs;
    if (elapsedMs >= kPreviewTimeoutMs) {
      process.kill();
      process.waitForFinished(3000);
      if (errorMessage) *errorMessage = QStringLiteral("YouTube preview timed out.");
      return false;
    }
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      *errorMessage = firstLine(tail);
      if (errorMessage->isEmpty()) {
        *errorMessage = QStringLiteral("YouTube preview failed.");
      }
    }
    return false;
  }

  if (!QFileInfo::exists(absolutePreviewPath)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("The YouTube preview did not produce a WAV file.");
    }
    return false;
  }

  if (previewPath) {
    *previewPath = absolutePreviewPath;
  }
  return true;
}

}  // namespace rpsu
