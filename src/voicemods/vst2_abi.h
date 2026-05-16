// Minimal VST 2.4 ABI definitions for plugin hosting.
//
// Clean-room declarations based on the well-known, publicly documented
// VST 2 host ABI. Only the symbols actually used by the host are defined;
// this file deliberately does not include any Steinberg SDK headers.
//
// The struct/opcode layout follows the published VST 2.4 specification used
// by countless open-source hosts (e.g. Carla, LMMS, Audacity, FluidSynth).

#pragma once

#include <cstdint>

extern "C" {

struct VstAEffect;

using VstHostCallback = intptr_t (*)(VstAEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt);
using VstDispatcherProc = intptr_t (*)(VstAEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt);
using VstProcessProc = void (*)(VstAEffect* effect, float** inputs, float** outputs, int32_t sampleFrames);
using VstProcessDoubleProc = void (*)(VstAEffect* effect, double** inputs, double** outputs, int32_t sampleFrames);
using VstSetParameterProc = void (*)(VstAEffect* effect, int32_t index, float parameter);
using VstGetParameterProc = float (*)(VstAEffect* effect, int32_t index);

constexpr int32_t kVstMagic = 0x56737450;  // 'VstP'

struct VstAEffect {
  int32_t magic;
  VstDispatcherProc dispatcher;
  VstProcessProc process;           // deprecated (additive), kept for ABI
  VstSetParameterProc setParameter;
  VstGetParameterProc getParameter;
  int32_t numPrograms;
  int32_t numParams;
  int32_t numInputs;
  int32_t numOutputs;
  int32_t flags;
  intptr_t resvd1;
  intptr_t resvd2;
  int32_t initialDelay;
  int32_t realQualities;
  int32_t offQualities;
  float ioRatio;
  void* object;
  void* user;
  int32_t uniqueID;
  int32_t version;
  VstProcessProc processReplacing;
  VstProcessDoubleProc processDoubleReplacing;
  char future[56];
};

// AEffect.flags bits
constexpr int32_t effFlagsHasEditor          = 1 << 0;
constexpr int32_t effFlagsCanReplacing       = 1 << 4;
constexpr int32_t effFlagsProgramChunks      = 1 << 5;
constexpr int32_t effFlagsIsSynth            = 1 << 8;
constexpr int32_t effFlagsNoSoundInStop      = 1 << 9;
constexpr int32_t effFlagsCanDoubleReplacing = 1 << 12;

// effOpcodes (host -> plugin)
constexpr int32_t effOpen               = 0;
constexpr int32_t effClose              = 1;
constexpr int32_t effSetProgram         = 2;
constexpr int32_t effGetProgram         = 3;
constexpr int32_t effGetProgramName     = 5;
constexpr int32_t effGetParamLabel      = 6;
constexpr int32_t effGetParamDisplay    = 7;
constexpr int32_t effGetParamName       = 8;
constexpr int32_t effSetSampleRate      = 10;
constexpr int32_t effSetBlockSize       = 11;
constexpr int32_t effMainsChanged       = 12;
constexpr int32_t effEditGetRect        = 13;
constexpr int32_t effEditOpen           = 14;
constexpr int32_t effEditClose          = 15;
constexpr int32_t effEditIdle           = 19;
constexpr int32_t effProcessEvents      = 25;
constexpr int32_t effGetPlugCategory    = 35;
constexpr int32_t effGetEffectName      = 45;
constexpr int32_t effGetVendorString    = 47;
constexpr int32_t effGetProductString   = 48;
constexpr int32_t effGetVendorVersion   = 49;
constexpr int32_t effCanDo              = 51;
constexpr int32_t effIdle               = 53;
constexpr int32_t effGetVstVersion      = 58;

// audioMaster opcodes (plugin -> host)
constexpr int32_t audioMasterAutomate                  = 0;
constexpr int32_t audioMasterVersion                   = 1;
constexpr int32_t audioMasterCurrentId                 = 2;
constexpr int32_t audioMasterIdle                      = 3;
constexpr int32_t audioMasterGetTime                   = 7;
constexpr int32_t audioMasterProcessEvents             = 8;
constexpr int32_t audioMasterIOChanged                 = 13;
constexpr int32_t audioMasterSizeWindow                = 15;
constexpr int32_t audioMasterGetSampleRate             = 16;
constexpr int32_t audioMasterGetBlockSize              = 17;
constexpr int32_t audioMasterGetInputLatency           = 18;
constexpr int32_t audioMasterGetOutputLatency          = 19;
constexpr int32_t audioMasterGetCurrentProcessLevel    = 23;
constexpr int32_t audioMasterGetAutomationState        = 24;
constexpr int32_t audioMasterGetVendorString           = 32;
constexpr int32_t audioMasterGetProductString          = 33;
constexpr int32_t audioMasterGetVendorVersion          = 34;
constexpr int32_t audioMasterCanDo                     = 37;
constexpr int32_t audioMasterGetLanguage               = 38;
constexpr int32_t audioMasterUpdateDisplay             = 42;
constexpr int32_t audioMasterBeginEdit                 = 43;
constexpr int32_t audioMasterEndEdit                   = 44;

using VstPluginMainProc = VstAEffect* (*)(VstHostCallback host);

}  // extern "C"
