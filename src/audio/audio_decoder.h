#pragma once

#include <QString>

#include "src/audio/wav_decoder.h"

namespace rpsu {

class AudioDecoder {
 public:
  static bool decodeFile(const QString& path, DecodedBuffer& output, QString* errorMessage);
};

}  // namespace rpsu
