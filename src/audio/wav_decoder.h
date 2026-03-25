#pragma once

#include <QString>
#include <QVector>

namespace rpsu {

struct DecodedBuffer {
  int sampleRate = 48000;
  QVector<float> monoSamples;
};

class WavDecoder {
 public:
  static bool decodeFile(const QString& path, DecodedBuffer& output, QString* errorMessage);
};

}  // namespace rpsu
