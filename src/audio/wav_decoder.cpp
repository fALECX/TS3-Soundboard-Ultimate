#include "src/audio/wav_decoder.h"

#include <QFile>

#include <cstring>

namespace rpsu {

namespace {

template <typename T>
T readLE(const char* data) {
  T value = 0;
  std::memcpy(&value, data, sizeof(T));
  return value;
}

}  // namespace

bool WavDecoder::decodeFile(const QString& path, DecodedBuffer& output, QString* errorMessage) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Could not open file.");
    }
    return false;
  }

  const QByteArray bytes = file.readAll();
  if (bytes.size() < 44 || bytes.mid(0, 4) != "RIFF" || bytes.mid(8, 4) != "WAVE") {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Only PCM WAV files are currently supported.");
    }
    return false;
  }

  int format = 0;
  int channels = 0;
  int sampleRate = 48000;
  int bitsPerSample = 0;
  QByteArray pcm;

  int offset = 12;
  while (offset + 8 <= bytes.size()) {
    const QByteArray chunkId = bytes.mid(offset, 4);
    const quint32 chunkSize = readLE<quint32>(bytes.constData() + offset + 4);
    const int dataOffset = offset + 8;
    if (dataOffset + static_cast<int>(chunkSize) > bytes.size()) {
      break;
    }

    if (chunkId == "fmt ") {
      format = readLE<quint16>(bytes.constData() + dataOffset);
      channels = readLE<quint16>(bytes.constData() + dataOffset + 2);
      sampleRate = static_cast<int>(readLE<quint32>(bytes.constData() + dataOffset + 4));
      bitsPerSample = readLE<quint16>(bytes.constData() + dataOffset + 14);
    } else if (chunkId == "data") {
      pcm = bytes.mid(dataOffset, static_cast<int>(chunkSize));
    }

    offset = dataOffset + static_cast<int>(chunkSize);
    if (offset % 2 != 0) {
      ++offset;
    }
  }

  if (format != 1 || channels <= 0 || bitsPerSample != 16 || pcm.isEmpty()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Unsupported WAV encoding.");
    }
    return false;
  }

  const int frameCount = pcm.size() / (channels * 2);
  output.sampleRate = sampleRate;
  output.monoSamples.resize(frameCount);

  const short* raw = reinterpret_cast<const short*>(pcm.constData());
  for (int frame = 0; frame < frameCount; ++frame) {
    float accum = 0.0f;
    for (int channel = 0; channel < channels; ++channel) {
      const int index = frame * channels + channel;
      accum += static_cast<float>(raw[index]) / 32768.0f;
    }
    output.monoSamples[frame] = accum / static_cast<float>(channels);
  }

  return true;
}

}  // namespace rpsu
