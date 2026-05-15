#include "src/engine/upstream/SampleBuffer.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

SampleBuffer::SampleBuffer(int channels, size_t maxSize)
    : m_channels(channels), m_maxSize(maxSize), m_readOffset(0), m_cbProd(nullptr), m_cbCons(nullptr) {
  if (m_maxSize > 0) {
    m_buf.reserve(m_maxSize * m_channels);
  }
}

void SampleBuffer::setOnProduce(ProduceCallback* cb) {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  m_cbProd = cb;
}

SampleBuffer::ProduceCallback* SampleBuffer::getOnProduce() const {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  return m_cbProd;
}

void SampleBuffer::setOnConsume(ConsumeCallback* cb) {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  m_cbCons = cb;
}

SampleBuffer::ConsumeCallback* SampleBuffer::getOnConsume() const {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  return m_cbCons;
}

int SampleBuffer::avail() const {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  return static_cast<int>((m_buf.size() - m_readOffset) / m_channels);
}

int SampleBuffer::channels() const {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  return m_channels;
}

size_t SampleBuffer::maxSize() const {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  return m_maxSize;
}

void SampleBuffer::produce(const short* samples, int count) {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  if (m_maxSize == 0 || avail() < static_cast<int>(m_maxSize)) {
    if (m_readOffset > 0 && m_readOffset >= m_buf.size() / 2) {
      m_buf.erase(m_buf.begin(), m_buf.begin() + static_cast<std::ptrdiff_t>(m_readOffset));
      m_readOffset = 0;
    }
    m_buf.insert(m_buf.end(), samples, samples + (count * m_channels));
    if (m_cbProd) {
      m_cbProd->onProduceSamples(samples, count, this);
    }
  }
}

int SampleBuffer::consume(short* samples, int maxCount, bool eraseConsumed) {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  const int count = std::min(avail(), maxCount);
  const size_t shorts = count * m_channels;
  if (samples) {
    std::memcpy(samples, m_buf.data() + m_readOffset, shorts * sizeof(short));
  }
  if (eraseConsumed) {
    m_readOffset += shorts;
    if (m_readOffset >= m_buf.size()) {
      m_buf.clear();
      m_readOffset = 0;
    } else if (m_readOffset >= m_buf.size() / 2) {
      m_buf.erase(m_buf.begin(), m_buf.begin() + static_cast<std::ptrdiff_t>(m_readOffset));
      m_readOffset = 0;
    }
  }
  if (m_cbCons) {
    m_cbCons->onConsumeSamples(samples, count, this);
  }
  return count;
}

int SampleBuffer::sampleSize() const {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  return 2 * m_channels;
}

short* SampleBuffer::getBufferData() {
  assert(!m_mutex.try_lock() && "Mutex not locked");
  return m_buf.data() + m_readOffset;
}

const std::mutex& SampleBuffer::getMutex() const {
  return m_mutex;
}

std::mutex& SampleBuffer::getMutex() {
  return m_mutex;
}
