#include "src/voicemods/vst2_host.h"

#include "src/voicemods/vst2_abi.h"

#include <QDebug>
#include <QFileInfo>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace rpsu {

namespace {

constexpr int32_t kHostVstVersion = 2400;

// Per-thread "current host" pointer so the C callback below can find the
// owning Vst2Host instance. VST2 plugins call back into the host during
// effOpen / effMainsChanged, so we need a way to route messages without
// holding global state across multiple hosts.
thread_local Vst2Host* tlCurrentHost = nullptr;

intptr_t hostCallback(VstAEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt) {
  (void)effect;
  (void)index;
  (void)value;
  (void)opt;

  switch (opcode) {
    case audioMasterVersion:
      return kHostVstVersion;
    case audioMasterCurrentId:
      return 0;
    case audioMasterIdle:
      return 0;
    case audioMasterGetSampleRate:
      return tlCurrentHost ? static_cast<intptr_t>(tlCurrentHost->effectName().isEmpty() ? 48000 : 48000) : 48000;
    case audioMasterGetBlockSize:
      return 4096;
    case audioMasterGetVendorString:
      if (ptr) std::strncpy(static_cast<char*>(ptr), "RP Soundboard Ultimate", 64);
      return 1;
    case audioMasterGetProductString:
      if (ptr) std::strncpy(static_cast<char*>(ptr), "RPSU Voicemod Host", 64);
      return 1;
    case audioMasterGetVendorVersion:
      return 1;
    case audioMasterCanDo:
      if (ptr) {
        const char* req = static_cast<const char*>(ptr);
        if (std::strcmp(req, "sendVstEvents") == 0) return 0;
        if (std::strcmp(req, "sendVstMidiEvent") == 0) return 0;
        if (std::strcmp(req, "sendVstTimeInfo") == 0) return 0;
        if (std::strcmp(req, "receiveVstEvents") == 0) return 0;
        if (std::strcmp(req, "receiveVstMidiEvent") == 0) return 0;
        if (std::strcmp(req, "reportConnectionChanges") == 0) return 0;
        if (std::strcmp(req, "acceptIOChanges") == 0) return 1;
        if (std::strcmp(req, "sizeWindow") == 0) return 0;
        if (std::strcmp(req, "asyncProcessing") == 0) return 0;
        if (std::strcmp(req, "supplyIdle") == 0) return 1;
      }
      return 0;
    case audioMasterGetLanguage:
      return 1;  // kVstLangEnglish
    case audioMasterGetCurrentProcessLevel:
      return 2;  // realtime processing
    case audioMasterGetAutomationState:
      return 1;  // off
    case audioMasterAutomate:
    case audioMasterIOChanged:
    case audioMasterUpdateDisplay:
    case audioMasterBeginEdit:
    case audioMasterEndEdit:
    default:
      return 0;
  }
}

QString readPluginString(const VstAEffect* effect, int32_t opcode, int32_t index = 0, int bufferSize = 256) {
  if (!effect) return QString();
  std::vector<char> buf(bufferSize, 0);
  effect->dispatcher(const_cast<VstAEffect*>(effect), opcode, index, 0, buf.data(), 0.0f);
  buf.back() = 0;
  return QString::fromLocal8Bit(buf.data());
}

}  // namespace

Vst2Host::Vst2Host() = default;

Vst2Host::~Vst2Host() {
  unload();
}

intptr_t Vst2Host::dispatch(int opcode, int index, intptr_t value, void* ptr, float opt) const {
  if (!effect_) return 0;
  return effect_->dispatcher(effect_, opcode, index, value, ptr, opt);
}

bool Vst2Host::load(const QString& dllPath, float sampleRate, int maxBlockSize, QString* errorMessage) {
  unload();

  const QFileInfo fi(dllPath);
  if (!fi.exists() || !fi.isFile()) {
    if (errorMessage) *errorMessage = QStringLiteral("VST DLL does not exist: %1").arg(dllPath);
    return false;
  }

  const std::wstring wpath = dllPath.toStdWString();
  HMODULE module = ::LoadLibraryW(wpath.c_str());
  if (!module) {
    const DWORD err = ::GetLastError();
    if (errorMessage) {
      *errorMessage = QStringLiteral("LoadLibrary failed for %1 (err=%2)")
                          .arg(fi.fileName())
                          .arg(static_cast<quint32>(err));
    }
    return false;
  }

  // VST2 plugins typically export VSTPluginMain; some older ones export "main".
  auto entry = reinterpret_cast<VstPluginMainProc>(::GetProcAddress(module, "VSTPluginMain"));
  if (!entry) {
    entry = reinterpret_cast<VstPluginMainProc>(::GetProcAddress(module, "main"));
  }
  if (!entry) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("DLL %1 has no VSTPluginMain/main export (not a VST2 plugin?)")
                          .arg(fi.fileName());
    }
    ::FreeLibrary(module);
    return false;
  }

  tlCurrentHost = this;
  VstAEffect* effect = entry(&hostCallback);
  tlCurrentHost = nullptr;
  if (!effect || effect->magic != kVstMagic) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Plugin %1 returned an invalid AEffect (magic mismatch).")
                          .arg(fi.fileName());
    }
    ::FreeLibrary(module);
    return false;
  }

  dllHandle_ = module;
  effect_ = effect;
  sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
  maxBlock_ = std::max(64, maxBlockSize);

  // Initialize the plugin: effOpen -> setSampleRate -> setBlockSize -> resume.
  tlCurrentHost = this;
  dispatch(effOpen);
  dispatch(effSetSampleRate, 0, 0, nullptr, sampleRate_);
  dispatch(effSetBlockSize, 0, maxBlock_);
  dispatch(effMainsChanged, 0, 1);  // resume
  resumed_ = true;

  effectName_ = readPluginString(effect_, effGetEffectName, 0, 64).trimmed();
  if (effectName_.isEmpty()) effectName_ = fi.completeBaseName();
  vendorString_ = readPluginString(effect_, effGetVendorString, 0, 64).trimmed();
  productString_ = readPluginString(effect_, effGetProductString, 0, 64).trimmed();
  tlCurrentHost = nullptr;

  return true;
}

void Vst2Host::unload() {
  if (effect_) {
    tlCurrentHost = this;
    if (resumed_) {
      dispatch(effMainsChanged, 0, 0);  // suspend
      resumed_ = false;
    }
    dispatch(effClose);
    tlCurrentHost = nullptr;
    effect_ = nullptr;
  }
  if (dllHandle_) {
    ::FreeLibrary(static_cast<HMODULE>(dllHandle_));
    dllHandle_ = nullptr;
  }
  effectName_.clear();
  vendorString_.clear();
  productString_.clear();
  scratchFrames_ = 0;
  scratchChannels_ = 0;
  scratchIn_.clear();
  scratchOut_.clear();
  inPtrs_.clear();
  outPtrs_.clear();
}

int Vst2Host::numParams() const { return effect_ ? effect_->numParams : 0; }
int Vst2Host::numInputs() const { return effect_ ? effect_->numInputs : 0; }
int Vst2Host::numOutputs() const { return effect_ ? effect_->numOutputs : 0; }

float Vst2Host::getParameter(int index) const {
  if (!effect_ || index < 0 || index >= effect_->numParams) return 0.0f;
  return effect_->getParameter(effect_, index);
}

void Vst2Host::setParameter(int index, float value) {
  if (!effect_ || index < 0 || index >= effect_->numParams) return;
  effect_->setParameter(effect_, index, std::clamp(value, 0.0f, 1.0f));
}

QString Vst2Host::parameterName(int index) const {
  if (!effect_ || index < 0 || index >= effect_->numParams) return QString();
  return readPluginString(effect_, effGetParamName, index, 64).trimmed();
}

QString Vst2Host::parameterDisplay(int index) const {
  if (!effect_ || index < 0 || index >= effect_->numParams) return QString();
  return readPluginString(effect_, effGetParamDisplay, index, 64).trimmed();
}

void Vst2Host::randomizeParameters(unsigned int seed) {
  if (!effect_) return;
  QRandomGenerator rng = seed ? QRandomGenerator(seed) : *QRandomGenerator::global();
  const int count = effect_->numParams;
  for (int i = 0; i < count; ++i) {
    const float v = rng.bounded(1.0);
    effect_->setParameter(effect_, i, v);
  }
}

void Vst2Host::ensureScratch(int frames, int channels) {
  const int wantInChannels = std::max(channels, effect_ ? effect_->numInputs : 0);
  const int wantOutChannels = std::max(channels, effect_ ? effect_->numOutputs : 0);
  const int wantChannels = std::max({channels, wantInChannels, wantOutChannels, 1});
  if (frames <= scratchFrames_ && wantChannels <= scratchChannels_) return;
  scratchFrames_ = std::max(frames, scratchFrames_);
  scratchChannels_ = std::max(wantChannels, scratchChannels_);
  scratchIn_.assign(static_cast<size_t>(scratchFrames_) * scratchChannels_, 0.0f);
  scratchOut_.assign(static_cast<size_t>(scratchFrames_) * scratchChannels_, 0.0f);
  inPtrs_.resize(scratchChannels_);
  outPtrs_.resize(scratchChannels_);
  for (int c = 0; c < scratchChannels_; ++c) {
    inPtrs_[c] = scratchIn_.data() + static_cast<size_t>(c) * scratchFrames_;
    outPtrs_[c] = scratchOut_.data() + static_cast<size_t>(c) * scratchFrames_;
  }
}

bool Vst2Host::processInPlaceInt16(short* samples, int frames, int channels) {
  if (!effect_ || frames <= 0 || channels <= 0 || !samples) return false;
  if (!effect_->processReplacing) return false;
  if (frames > maxBlock_) {
    // Re-set block size if the host hands us a bigger buffer than we declared.
    maxBlock_ = frames;
    dispatch(effMainsChanged, 0, 0);
    dispatch(effSetBlockSize, 0, maxBlock_);
    dispatch(effMainsChanged, 0, 1);
  }
  ensureScratch(frames, channels);

  const int pluginIns = std::max(1, effect_->numInputs);
  const int pluginOuts = std::max(1, effect_->numOutputs);

  // Deinterleave int16 -> float, duplicating channels if the plugin expects
  // more inputs than we have (typical: mono mic -> stereo plugin).
  constexpr float kInvScale = 1.0f / 32768.0f;
  for (int c = 0; c < pluginIns; ++c) {
    float* dst = inPtrs_[c];
    const int srcChannel = std::min(c, channels - 1);
    for (int i = 0; i < frames; ++i) {
      dst[i] = static_cast<float>(samples[i * channels + srcChannel]) * kInvScale;
    }
  }

  // Clear any extra output buffers so the plugin starts with silence there.
  for (int c = 0; c < pluginOuts; ++c) {
    std::memset(outPtrs_[c], 0, static_cast<size_t>(frames) * sizeof(float));
  }

  tlCurrentHost = this;
  effect_->processReplacing(effect_, inPtrs_.data(), outPtrs_.data(), frames);
  tlCurrentHost = nullptr;

  // Reinterleave float -> int16, mapping plugin outputs back to mic channels.
  for (int c = 0; c < channels; ++c) {
    const int srcChannel = std::min(c, pluginOuts - 1);
    const float* src = outPtrs_[srcChannel];
    for (int i = 0; i < frames; ++i) {
      float v = src[i] * 32768.0f;
      v = std::clamp(v, -32768.0f, 32767.0f);
      samples[i * channels + c] = static_cast<short>(std::lrintf(v));
    }
  }

  return true;
}

}  // namespace rpsu
