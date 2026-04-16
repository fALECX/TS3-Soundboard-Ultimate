#pragma once

#include <cassert>
#include <mutex>
#include <vector>

#include "src/engine/upstream/SampleProducer.h"

class SampleBuffer : public SampleProducer {
 public:
  using Mutex = std::mutex;
  using Lock = std::lock_guard<Mutex>;

  class ProduceCallback {
   public:
    virtual ~ProduceCallback() = default;
    virtual void onProduceSamples(const short* samples, int count, SampleBuffer* caller) = 0;
  };

  class ConsumeCallback {
   public:
    virtual ~ConsumeCallback() = default;
    virtual void onConsumeSamples(const short* samples, int count, SampleBuffer* caller) = 0;
  };

  SampleBuffer(int channels, size_t maxSize = 0);

  void setOnProduce(ProduceCallback* cb);
  ConsumeCallback* getOnProduce() const;
  void setOnConsume(ConsumeCallback* cb);
  ConsumeCallback* getOnConsume() const;
  int avail() const;
  int channels() const;
  size_t maxSize() const;
  void produce(const short* samples, int count) override;
  int consume(short* samples, int maxCount, bool eraseConsumed = true);
  int sampleSize() const;
  short* getBufferData();
  const std::mutex& getMutex() const;
  std::mutex& getMutex();

 private:
  const int m_channels;
  const size_t m_maxSize;
  std::vector<short> m_buf;
  mutable std::mutex m_mutex;
  ProduceCallback* m_cbProd;
  ConsumeCallback* m_cbCons;
};
