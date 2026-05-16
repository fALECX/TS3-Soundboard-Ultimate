#pragma once

#include <QString>
#include <atomic>
#include <memory>
#include <mutex>

namespace rpsu {

class Vst2Host;

// Singleton holding the currently-active voicemod VST plugin chain
// (chain length is 1 in this first version).
//
// processCapture() is called from the TS3 audio thread; load/unload/randomize
// are called from the Qt UI thread. A single mutex guards the host_ pointer
// and the VST instance state.
class VoicemodManager {
 public:
  static VoicemodManager& instance();

  // ---- Audio-thread API --------------------------------------------------
  // Returns true if the buffer was actually modified by a VST.
  bool processCapture(short* samples, int frames, int channels);

  // ---- UI-thread API -----------------------------------------------------
  bool loadVst(const QString& dllPath, QString* errorMessage = nullptr);
  void unloadVst();
  bool isVstLoaded() const;
  QString currentVstPath() const;
  QString currentVstName() const;
  QString currentVstVendor() const;
  int currentVstParamCount() const;

  void setEnabled(bool enabled);
  bool isEnabled() const { return enabled_.load(std::memory_order_relaxed); }

  void randomizeParameters();

  // The capture buffer sample rate is published by the runtime when the TS3
  // pipeline starts up. Defaults to 48000 Hz, which matches the rest of the
  // soundboard's audio path.
  void setSampleRate(float sr);
  float sampleRate() const;

 private:
  VoicemodManager();
  ~VoicemodManager();

  VoicemodManager(const VoicemodManager&) = delete;
  VoicemodManager& operator=(const VoicemodManager&) = delete;

  mutable std::mutex mutex_;
  std::unique_ptr<Vst2Host> host_;
  QString vstPath_;
  std::atomic<bool> enabled_{false};
  std::atomic<bool> hasHost_{false};
  float sampleRate_ = 48000.0f;
  int maxBlock_ = 4096;
};

}  // namespace rpsu
