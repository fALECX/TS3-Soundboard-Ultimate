#pragma once

#include "src/engine/upstream/SampleProducer.h"

class SampleSource {
 public:
  virtual ~SampleSource() = default;
  virtual int readSamples(SampleProducer* sampleBuffer) = 0;
};
