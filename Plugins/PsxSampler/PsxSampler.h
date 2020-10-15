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
    kParamNoteMin,
    kParamNoteMax,
    kNumParams
};

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
    // Maximum number of active voices: this is the hardware limit of the PS1
    static constexpr uint32_t kMaxVoices = 24;

    PsxSampler(const InstanceInfo& info) noexcept;

    virtual void ProcessBlock(sample** pInputs, sample** pOutputs, int numFrames) noexcept override;
    virtual void ProcessMidiMsg(const IMidiMsg& msg) noexcept override;
    virtual void OnIdle() noexcept override;
    virtual bool SerializeState(IByteChunk &chunk) const noexcept override;
    virtual int UnserializeState(const IByteChunk &chunk, int startPos) noexcept override;

private:
    // Information for a playing voice
    struct VoiceInfo {
        uint16_t midiNote;            // The note played
        uint16_t midiVelocity;        // 0-127 velocity
        uint32_t numSamplesActive;    // Number of samples the voice has been active for
    };

    Spu::Core                       mSpu;
    mutable std::recursive_mutex    mSpuMutex;
    uint32_t                        mCurMidiPitchBend;        // Current MIDI pitch bend value, a 14-bit value: 0x2000 = center, 0x0000 = lowest, 0x3FFF = highest
    VoiceInfo                       mVoiceInfos[kMaxVoices];
    IPeakSender<2>                  mMeterSender;
    IMidiQueue                      mMidiQueue;

    void DefinePluginParams() noexcept;
    void DoEditorSetup() noexcept;
    void DoDspSetup() noexcept;
    virtual void InformHostOfParamChange(int idx, double normalizedValue) noexcept override;
    virtual void OnRestoreState() noexcept override;
    void AddSampleTerminator() noexcept;
    void ProcessMidiQueue() noexcept;
    void ProcessQueuedMidiMsg(const IMidiMsg& msg) noexcept;
    void ProcessMidiNoteOn(const uint8_t note, const uint8_t velocity) noexcept;
    void ProcessMidiNoteOff(const uint8_t note) noexcept;
    void ProcessMidiPitchBend(const uint16_t pitchBend) noexcept;
    void ProcessMidiAllNotesOff() noexcept;
    void UpdateSpuVoicesFromParams() noexcept;
    void UpdateSpuVoiceFromParams(const uint32_t voiceIdx) noexcept;
    static uint16_t CalcSpuVoiceSampleRate(const uint32_t baseSampleRate, const float voiceNote) noexcept;
    static Spu::Volume CalcSpuVoiceVolume(const uint32_t volume, const uint32_t pan, const uint32_t velocity) noexcept;
    Spu::AdsrEnvelope GetCurrentSpuAdsrEnv() const noexcept;
    float GetCurrentPitchBendInNotes() const noexcept;
    void DoLoadVagFilePrompt(IGraphics& graphics) noexcept;
    void DoSaveVagFilePrompt(IGraphics& graphics) noexcept;
    void SetBaseNoteFromSampleRate() noexcept;
    void SetSampleRateFromBaseNote() noexcept;
    void DoNoteOffForOutOfRangeNotes() noexcept;
    void KeyOffAllSpuVoices() noexcept;
    void KillAllSpuVoices() noexcept;
};
