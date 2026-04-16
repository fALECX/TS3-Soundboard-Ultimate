#include "src/engine/upstream/inputfile.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>
#include <cstring>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

namespace {

QString resolveFfmpegExecutable() {
  QStringList candidates;

#ifdef _WIN32
  wchar_t modulePath[MAX_PATH] = {};
  if (GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH) > 0) {
    const QString runtimePath = QString::fromWCharArray(modulePath);
    const QDir runtimeDir = QFileInfo(runtimePath).absoluteDir();
    candidates << runtimeDir.filePath(QStringLiteral("ffmpeg.exe"));
    candidates << runtimeDir.filePath(QStringLiteral("ffmpeg"));
  }
#endif

  const QString appDir = QCoreApplication::applicationDirPath();
  if (!appDir.isEmpty()) {
    candidates << QDir(appDir).filePath(QStringLiteral("plugins/rp_soundboard_ultimate/ffmpeg.exe"));
    candidates << QDir(appDir).filePath(QStringLiteral("plugins/rp_soundboard_ultimate/ffmpeg"));
    candidates << QDir(appDir).filePath(QStringLiteral("ffmpeg.exe"));
    candidates << QDir(appDir).filePath(QStringLiteral("ffmpeg"));
  }

  candidates << QStandardPaths::findExecutable(QStringLiteral("ffmpeg.exe"))
             << QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));

  for (const QString& candidate : candidates) {
    if (!candidate.isEmpty() && QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
  return {};
}

class InputFileFFmpeg : public InputFile {
 public:
  explicit InputFileFFmpeg(const InputFileOptions& options)
      : m_options(options), m_opened(false), m_done(false), m_framePosition(0), m_sampleCount(0) {}

  int open(const char* filename, double startPosSeconds = 0.0, double playTimeSeconds = -1.0) override {
    close();
    const QString ffmpegPath = resolveFfmpegExecutable();
    if (ffmpegPath.isEmpty()) {
      return -1;
    }

    QStringList args;
    args << QStringLiteral("-v") << QStringLiteral("error");
    if (startPosSeconds > 0.0) {
      args << QStringLiteral("-ss") << QString::number(startPosSeconds, 'f', 3);
    }
    args << QStringLiteral("-i") << QString::fromUtf8(filename);
    if (playTimeSeconds > 0.0) {
      args << QStringLiteral("-t") << QString::number(playTimeSeconds, 'f', 3);
    }
    args << QStringLiteral("-vn")
         << QStringLiteral("-f") << QStringLiteral("s16le")
         << QStringLiteral("-acodec") << QStringLiteral("pcm_s16le")
         << QStringLiteral("-ac") << QString::number(m_options.getNumChannels())
         << QStringLiteral("-ar") << QString::number(m_options.outputSampleRate)
         << QStringLiteral("-");

    QProcess process;
    process.setProgram(ffmpegPath);
    process.setArguments(args);
    process.start();
    if (!process.waitForStarted()) {
      return -1;
    }
    if (!process.waitForFinished(-1) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
      return -1;
    }

    const QByteArray raw = process.readAllStandardOutput();
    if (raw.isEmpty()) {
      return -1;
    }

    m_pcm.resize(raw.size() / static_cast<int>(sizeof(short)));
    std::memcpy(m_pcm.data(), raw.constData(), raw.size());
    m_framePosition = 0;
    m_sampleCount = static_cast<int64_t>(m_pcm.size() / m_options.getNumChannels());
    m_done = false;
    m_opened = true;
    return 0;
  }

  int close() override {
    m_pcm.clear();
    m_framePosition = 0;
    m_sampleCount = 0;
    m_done = false;
    m_opened = false;
    return 0;
  }

  int readSamples(SampleProducer* sampleBuffer) override {
    if (!m_opened || m_done || !sampleBuffer) {
      return 0;
    }

    constexpr int kChunkFrames = 4096;
    const int64_t remainingFrames = m_sampleCount - m_framePosition;
    if (remainingFrames <= 0) {
      m_done = true;
      return 0;
    }

    const int frames = static_cast<int>(std::min<int64_t>(remainingFrames, kChunkFrames));
    const short* data = m_pcm.data() + (m_framePosition * m_options.getNumChannels());
    sampleBuffer->produce(data, frames);
    m_framePosition += frames;
    if (m_framePosition >= m_sampleCount) {
      m_done = true;
    }
    return frames;
  }

  bool done() const override {
    return m_done;
  }

  int seek(double seconds) override {
    if (!m_opened) {
      return -1;
    }
    const int64_t target = static_cast<int64_t>(seconds * m_options.outputSampleRate);
    m_framePosition = std::clamp<int64_t>(target, 0, m_sampleCount);
    m_done = m_framePosition >= m_sampleCount;
    return 0;
  }

  int64_t outputSamplesEstimation() const override {
    return m_sampleCount;
  }

 private:
  InputFileOptions m_options;
  std::vector<short> m_pcm;
  bool m_opened;
  bool m_done;
  int64_t m_framePosition;
  int64_t m_sampleCount;
};

}  // namespace

InputFile* CreateInputFileFFmpeg(InputFileOptions options) {
  return new InputFileFFmpeg(options);
}
