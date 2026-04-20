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

namespace {

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

}  // namespace

QString YouTubeService::resolveYtDlpPath(QString* errorMessage) const {
  const QStringList bundledCandidates = {
    QDir(moduleDirectory()).filePath(QStringLiteral("yt-dlp.exe")),
    QDir(moduleDirectory()).filePath(QStringLiteral("rp_soundboard_ultimate/yt-dlp.exe"))
  };
  for (const QString& bundledPath : bundledCandidates) {
    if (QFileInfo::exists(bundledPath)) {
      return bundledPath;
    }
  }

  const QString pathTool = runWhereCommand(QStringLiteral("yt-dlp"));
  if (!pathTool.isEmpty()) {
    return pathTool;
  }

  if (errorMessage) {
    *errorMessage = QStringLiteral("yt-dlp.exe is missing from the packaged dashboard files.");
  }
  return QString();
}

QString YouTubeService::resolveFfmpegPath(QString* errorMessage) const {
  const QStringList bundledCandidates = {
    QDir(moduleDirectory()).filePath(QStringLiteral("ffmpeg.exe")),
    QDir(moduleDirectory()).filePath(QStringLiteral("rp_soundboard_ultimate/ffmpeg.exe"))
  };
  for (const QString& bundledPath : bundledCandidates) {
    if (QFileInfo::exists(bundledPath)) {
      return bundledPath;
    }
  }

  const QString localPath = runWhereCommand(QStringLiteral("ffmpeg"));
  if (!localPath.isEmpty()) {
    return localPath;
  }

  const QString wingetPath = findWingetTool(QStringLiteral("ffmpeg.exe"));
  if (!wingetPath.isEmpty()) {
    return wingetPath;
  }

  if (errorMessage) {
    *errorMessage = QStringLiteral("ffmpeg.exe was not found. Install FFmpeg or add it to PATH.");
  }
  return QString();
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
  process.start(
    ytDlpPath,
    {
      QStringLiteral("ytsearch%1:%2").arg(QString::number(qMax(1, limit)), query.trimmed()),
      QStringLiteral("--dump-json"),
      QStringLiteral("--flat-playlist"),
      QStringLiteral("--no-warnings")
    }
  );

  if (!process.waitForFinished(20000)) {
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

  const QString safeBase = sanitizeFilenameBase(result.title);
  const QString desiredFilename = ensureUniqueFilename(existingNames, safeBase + QStringLiteral(".wav"));
  const QString outputTemplate = QDir(soundsDir).filePath(QFileInfo(desiredFilename).completeBaseName() + QStringLiteral(".%(ext)s"));

  QProcess process;
  process.setWorkingDirectory(soundsDir);
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
      QStringLiteral("-o"),
      outputTemplate
    }
  );

  if (!process.waitForFinished(180000)) {
    process.kill();
    if (errorMessage) {
      *errorMessage = QStringLiteral("YouTube download timed out.");
    }
    return false;
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      *errorMessage = firstLine(QString::fromLocal8Bit(process.readAllStandardError()));
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
  process.setWorkingDirectory(previewDir);
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

  if (!process.waitForFinished(180000)) {
    process.kill();
    if (errorMessage) {
      *errorMessage = QStringLiteral("YouTube preview timed out.");
    }
    return false;
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      *errorMessage = firstLine(QString::fromLocal8Bit(process.readAllStandardError()));
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
