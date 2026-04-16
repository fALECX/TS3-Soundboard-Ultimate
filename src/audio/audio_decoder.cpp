#include "src/audio/audio_decoder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QTemporaryFile>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rpsu {

namespace {

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

QString resolveFfmpegExecutable(QString* errorMessage) {
  const QStringList candidates = {
    QDir(moduleDirectory()).filePath(QStringLiteral("ffmpeg.exe")),
    QDir(moduleDirectory()).filePath(QStringLiteral("rp_soundboard_ultimate/ffmpeg.exe")),
    QStandardPaths::findExecutable(QStringLiteral("ffmpeg.exe")),
    QStandardPaths::findExecutable(QStringLiteral("ffmpeg"))
  };

  for (const QString& candidate : candidates) {
    if (!candidate.isEmpty() && QFileInfo::exists(candidate)) {
      return candidate;
    }
  }

  if (errorMessage) {
    *errorMessage = QStringLiteral("ffmpeg.exe was not found. The plugin package should include it.");
  }
  return {};
}

}  // namespace

bool AudioDecoder::decodeFile(const QString& path, DecodedBuffer& output, QString* errorMessage) {
  if (WavDecoder::decodeFile(path, output, nullptr)) {
    return true;
  }

  QString ffmpegError;
  const QString ffmpegPath = resolveFfmpegExecutable(&ffmpegError);
  if (ffmpegPath.isEmpty()) {
    if (errorMessage) {
      *errorMessage = ffmpegError;
    }
    return false;
  }

  QTemporaryFile tempFile(QDir::temp().filePath(QStringLiteral("rpsu_decode_XXXXXX.wav")));
  tempFile.setAutoRemove(false);
  if (!tempFile.open()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Could not create a temporary decode file.");
    }
    return false;
  }

  const QString tempPath = tempFile.fileName();
  tempFile.close();

  QProcess process;
  process.setProgram(ffmpegPath);
  process.setArguments({
    QStringLiteral("-y"),
    QStringLiteral("-i"),
    path,
    QStringLiteral("-vn"),
    QStringLiteral("-ac"),
    QStringLiteral("1"),
    QStringLiteral("-ar"),
    QStringLiteral("48000"),
    QStringLiteral("-f"),
    QStringLiteral("wav"),
    tempPath
  });
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start();
  if (!process.waitForStarted()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Could not start ffmpeg.exe.");
    }
    QFile::remove(tempPath);
    return false;
  }

  if (!process.waitForFinished(-1) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    if (errorMessage) {
      const QString outputText = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
      *errorMessage = outputText.isEmpty()
                        ? QStringLiteral("ffmpeg.exe failed to decode the sound file.")
                        : QStringLiteral("ffmpeg.exe failed to decode the sound file: %1").arg(outputText);
    }
    QFile::remove(tempPath);
    return false;
  }

  const bool decoded = WavDecoder::decodeFile(tempPath, output, errorMessage);
  QFile::remove(tempPath);
  return decoded;
}

}  // namespace rpsu
