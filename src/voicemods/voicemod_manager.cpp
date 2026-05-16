#include "src/voicemods/voicemod_manager.h"

#include "src/voicemods/vst2_host.h"

namespace rpsu {

VoicemodManager& VoicemodManager::instance() {
  static VoicemodManager mgr;
  return mgr;
}

VoicemodManager::VoicemodManager() = default;
VoicemodManager::~VoicemodManager() = default;

bool VoicemodManager::processCapture(short* samples, int frames, int channels) {
  if (!enabled_.load(std::memory_order_relaxed)) return false;
  if (!hasHost_.load(std::memory_order_relaxed)) return false;
  // try_lock so the audio thread never blocks behind UI plugin loading.
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (!lock.owns_lock() || !host_) return false;
  return host_->processInPlaceInt16(samples, frames, channels);
}

bool VoicemodManager::loadVst(const QString& dllPath, QString* errorMessage) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto host = std::make_unique<Vst2Host>();
  if (!host->load(dllPath, sampleRate_, maxBlock_, errorMessage)) {
    return false;
  }
  host_ = std::move(host);
  vstPath_ = dllPath;
  hasHost_.store(true, std::memory_order_release);
  return true;
}

void VoicemodManager::unloadVst() {
  std::lock_guard<std::mutex> lock(mutex_);
  hasHost_.store(false, std::memory_order_release);
  host_.reset();
  vstPath_.clear();
}

bool VoicemodManager::isVstLoaded() const {
  return hasHost_.load(std::memory_order_acquire);
}

QString VoicemodManager::currentVstPath() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return vstPath_;
}

QString VoicemodManager::currentVstName() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return host_ ? host_->effectName() : QString();
}

QString VoicemodManager::currentVstVendor() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return host_ ? host_->vendorString() : QString();
}

int VoicemodManager::currentVstParamCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return host_ ? host_->numParams() : 0;
}

void VoicemodManager::setEnabled(bool enabled) {
  enabled_.store(enabled, std::memory_order_release);
}

void VoicemodManager::randomizeParameters() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (host_) host_->randomizeParameters();
}

void VoicemodManager::setSampleRate(float sr) {
  std::lock_guard<std::mutex> lock(mutex_);
  sampleRate_ = sr > 0.0f ? sr : 48000.0f;
}

float VoicemodManager::sampleRate() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return sampleRate_;
}

}  // namespace rpsu
