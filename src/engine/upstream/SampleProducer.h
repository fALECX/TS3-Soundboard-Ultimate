#pragma once

class SampleProducer {
 public:
  virtual ~SampleProducer() = default;
  virtual void produce(const short* samples, int count) = 0;
};
