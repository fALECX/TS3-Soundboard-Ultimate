#pragma once

#include <QString>
#include <vector>

struct VstAEffect;

namespace rpsu {

// Hosts a single VST 2.x effect plugin loaded from a DLL.
//
// Lifetime:
//   load()  -> ready to process()
//   unload() -> safe to load() again
//
// processInPlaceInt16() is intended to be called from the audio thread; load
// / unload / parameter mutation must NOT run concurrently with it. The
// VoicemodManager wrapping this class is responsible for that locking.
class Vst2Host {
 public:
  Vst2Host();
  ~Vst2Host();

  Vst2Host(const Vst2Host&) = delete;
  Vst2Host& operator=(const Vst2Host&) = delete;

  bool load(const QString& dllPath,
            float sampleRate,
            int maxBlockSize,
            QString* errorMessage = nullptr);
  void unload();
  bool isLoaded() const { return effect_ != nullptr; }

  // Processes one block of interleaved int16 audio in place.
  // Returns true if the buffer was modified.
  bool processInPlaceInt16(short* samples, int frames, int channels);

  int numParams() const;
  int numInputs() const;
  int numOutputs() const;

  float getParameter(int index) const;
  void setParameter(int index, float value);
  QString parameterName(int index) const;
  QString parameterDisplay(int index) const;

  QString effectName() const { return effectName_; }
  QString vendorString() const { return vendorString_; }
  QString productString() const { return productString_; }

  void randomizeParameters(unsigned int seed = 0);

 private:
  void ensureScratch(int frames, int channels);
  intptr_t dispatch(int opcode, int index = 0, intptr_t value = 0, void* ptr = nullptr, float opt = 0.0f) const;

  void* dllHandle_ = nullptr;
  VstAEffect* effect_ = nullptr;
  float sampleRate_ = 48000.0f;
  int maxBlock_ = 0;
  bool resumed_ = false;

  QString effectName_;
  QString vendorString_;
  QString productString_;

  // Deinterleave scratch buffers. Plain contiguous storage for input + output
  // floats; inPtrs_/outPtrs_ index into them per channel.
  std::vector<float> scratchIn_;
  std::vector<float> scratchOut_;
  std::vector<float*> inPtrs_;
  std::vector<float*> outPtrs_;
  int scratchFrames_ = 0;
  int scratchChannels_ = 0;
};

}  // namespace rpsu
