#pragma once

#include <mutex>
#include <thread>
#include <vector>

#include "src/engine/upstream/SampleProducer.h"

class SampleBuffer;
class SampleSource;

class SampleProducerThread : public SampleProducer {
  struct buffer_t {
    SampleBuffer* buffer;
    bool enabled;
  };

 public:
  SampleProducerThread();
  void addBuffer(SampleBuffer* buffer, bool enableBuffer = true);
  void remBuffer(SampleBuffer* buffer);
  void setBufferEnabled(SampleBuffer* buffer, bool enabled);
  void start();
  void stop(bool wait = true);
  bool isRunning();
  void setSource(SampleSource* source);

 private:
  void run();
  void threadFunc();
  bool singleBufferFill();
  void produce(const short* samples, int count) override;

  using Lock = std::lock_guard<std::mutex>;

  std::thread m_thread;
  SampleSource* volatile m_source;
  std::vector<buffer_t> m_buffers;
  bool m_running;
  volatile bool m_stop;
  std::mutex m_mutex;
};
