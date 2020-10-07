#pragma once

#include "IPlug_include_in_plug_hdr.h"

#include "IControls.h"
#include "../../PluginsCommon/Spu.h"
#include <mutex>

using namespace iplug;
using namespace igraphics;

//------------------------------------------------------------------------------------------------------------------------------------------
// All of the parameters used by the instrument.
// Note that some of these are purely informational, and don't actually affect anything.
// Sample rate and base note are also two views looking at the same information.
//------------------------------------------------------------------------------------------------------------------------------------------
enum EParams : uint32_t {
    kParamSampleRate,
    kParamBaseNote,
    kParamLengthInSamples,
    kParamLengthInBlocks,
    kParamLoopStartSample,
    kParamLoopEndSample,
    kParamVolume,
    kParamPan,
    kParamPitchstepUp,
    kParamPitchstepDown,
    kParamAttackStep,
    kParamAttackShift,
    kParamAttackIsExp,
    kParamDecayShift,
    kParamSustainLevel,
    kParamSustainStep,
    kParamSustainShift,
    kParamSustainDec,
    kParamSustainIsExp,
    kParamReleaseShift,
    kParamReleaseIsExp,
    kNumParams
};

#if IPLUG_DSP
    // Will use EParams in PsxSampler_DSP.h
    #include "PsxSampler_DSP.h"
#endif

//------------------------------------------------------------------------------------------------------------------------------------------
// UI control identifiers
//------------------------------------------------------------------------------------------------------------------------------------------
enum EControlTags : uint32_t {
    kCtrlTagMeter = 0,
    kCtrlTagKeyboard,
    kCtrlTagBender,
    kNumCtrlTags
};

//------------------------------------------------------------------------------------------------------------------------------------------
// Logic for the PlayStation 1 sampler instrument plugin
//------------------------------------------------------------------------------------------------------------------------------------------
class PsxSampler final : public Plugin {
public:
    PsxSampler(const InstanceInfo& info) noexcept;

    virtual void ProcessBlock(sample** pInputs, sample** pOutputs, int numFrames) noexcept override;
    virtual void ProcessMidiMsg(const IMidiMsg& msg) noexcept override;
    virtual void OnReset() noexcept override;
    virtual void OnParamChange(int paramIdx) noexcept override;
    virtual void OnIdle() noexcept override;
    virtual bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) noexcept override;

private:
    Spu::Core               mSpu;
    std::recursive_mutex    mSpuMutex;
    uint32_t                mNumSampleBlocks;
    PsxSamplerDSP<sample>   mDSP;
    IPeakSender<2>          mMeterSender;

    void DefinePluginParams() noexcept;
    void DoEditorSetup() noexcept;
    void DoDspSetup() noexcept;
    virtual void InformHostOfParamChange(int idx, double normalizedValue) noexcept override;
    virtual void OnRestoreState() noexcept override;
    void UpdateSpuVoicesFromParams() noexcept;
    void AddSampleTerminator() noexcept;
};
