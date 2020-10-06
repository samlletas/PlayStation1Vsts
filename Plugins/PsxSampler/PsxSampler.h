#pragma once

#include "IPlug_include_in_plug_hdr.h"

#include "IControls.h"
#include "../../PluginsCommon/Spu.h"
#include <mutex>

const int kNumPresets = 1;

enum EParams : uint32_t {
    kParamGain = 0,
    kParamNoteGlideTime,
    kParamAttack,
    kParamDecay,
    kParamSustain,
    kParamRelease,
    kParamLFOShape,
    kParamLFORateHz,
    kParamLFORateTempo,
    kParamLFORateMode,
    kParamLFODepth,
    kNumParams
};

#if IPLUG_DSP
    // will use EParams in PsxSampler_DSP.h
    #include "PsxSampler_DSP.h"
#endif

enum EControlTags : uint32_t {
    kCtrlTagMeter = 0,
    kCtrlTagScope,
    kCtrlTagRTText,
    kCtrlTagKeyboard,
    kCtrlTagBender,
    kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class PsxSampler final : public Plugin {
public:
    PsxSampler(const InstanceInfo& info) noexcept;

    #if IPLUG_DSP
        virtual void ProcessBlock(sample** pInputs, sample** pOutputs, int numFrames) noexcept override;
        virtual void ProcessMidiMsg(const IMidiMsg& msg) noexcept override;
        virtual void OnReset() noexcept override;
        virtual void OnParamChange(int paramIdx) noexcept override;
        virtual void OnIdle() noexcept override;
        virtual bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) noexcept override;
    #endif

private:
    #if IPLUG_DSP
        Spu::Core               mSpu;
        std::recursive_mutex    mSpuMutex;
        uint32_t                mNumSampleBlocks;
        PsxSamplerDSP<sample>   mDSP;
        IPeakSender<2>          mMeterSender;

        void DoDspSetup() noexcept;
        virtual void InformHostOfParamChange(int idx, double normalizedValue) noexcept override;
        virtual void OnRestoreState() noexcept override;
        void UpdateSpuRegistersFromParams() noexcept;
        void AddSampleTerminator() noexcept;
    #endif
};
