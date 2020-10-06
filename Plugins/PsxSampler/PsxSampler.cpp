#include "PsxSampler.h"
#include "IPlug_include_in_plug_src.h"
#include "LFO.h"

static constexpr uint32_t   kSpuRamSize     = 512 * 1024;   // SPU RAM size: this is the size that the PS1 had
static constexpr uint32_t   kSpuNumVoices   = 24;           // Maximum number of SPU voices: this is the hardware limit of the PS1

PsxSampler::PsxSampler(const InstanceInfo& info) noexcept
    : Plugin(info, MakeConfig(kNumParams, kNumPresets))
    , mSpu()
    , mSpuMutex()
    , mNumSampleBlocks(0)
    , mDSP{16}
    , mMeterSender()
{
    DefinePluginParams();
    DoEditorSetup();
}

void PsxSampler::ProcessBlock(sample** inputs, sample** outputs, int nFrames) noexcept {
    mDSP.ProcessBlock(nullptr, outputs, 2, nFrames, mTimeInfo.mPPQPos, mTimeInfo.mTransportIsRunning);
    mMeterSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
}

void PsxSampler::OnIdle() noexcept {
    mMeterSender.TransmitData(*this);
}

void PsxSampler::OnReset() noexcept {
    mDSP.Reset(GetSampleRate(), GetBlockSize());
}

void PsxSampler::ProcessMidiMsg(const IMidiMsg& msg) noexcept {
    int status = msg.StatusMsg();
    
    switch (status) {
        case IMidiMsg::kNoteOn:
        case IMidiMsg::kNoteOff:
        case IMidiMsg::kPolyAftertouch:
        case IMidiMsg::kControlChange:
        case IMidiMsg::kProgramChange:
        case IMidiMsg::kChannelAftertouch:
        case IMidiMsg::kPitchWheel:
            mDSP.ProcessMidiMsg(msg);
            SendMidiMsg(msg);
            break;
        
        default:
            break;
    }
}

void PsxSampler::OnParamChange(int paramIdx) noexcept {
    mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value());
}

bool PsxSampler::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) noexcept {
    if ((ctrlTag == kCtrlTagBender) && (msgTag == IWheelControl::kMessageTagSetPitchBendRange)) {
        const int bendRange = *static_cast<const int*>(pData);
        mDSP.mSynth.SetPitchBendRange(bendRange);
    }
    
    return false;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Defines the parameters used by the plugin
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::DefinePluginParams() noexcept {
    GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100.0, 0.01, "%");
    GetParam(kParamNoteGlideTime)->InitMilliseconds("Note Glide Time", 0., 0.0, 30.);
    GetParam(kParamAttack)->InitDouble("Attack", 10., 1., 1000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
    GetParam(kParamDecay)->InitDouble("Decay", 10., 1., 1000., 0.1, "ms", IParam::kFlagsNone, "ADSR", IParam::ShapePowCurve(3.));
    GetParam(kParamSustain)->InitDouble("Sustain", 50., 0., 100., 1, "%", IParam::kFlagsNone, "ADSR");
    GetParam(kParamRelease)->InitDouble("Release", 10., 2., 1000., 0.1, "ms", IParam::kFlagsNone, "ADSR");
    GetParam(kParamLFOShape)->InitEnum("LFO Shape", LFO<>::kTriangle, {LFO_SHAPE_VALIST});
    GetParam(kParamLFORateHz)->InitFrequency("LFO Rate", 1., 0.01, 40.);
    GetParam(kParamLFORateTempo)->InitEnum("LFO Rate", LFO<>::k1, {LFO_TEMPODIV_VALIST});
    GetParam(kParamLFORateMode)->InitBool("LFO Sync", true);
    GetParam(kParamLFODepth)->InitPercentage("LFO Depth");
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Setup controls for the plugin's GUI
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::DoEditorSetup() noexcept {
    mMakeGraphicsFunc = [&]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_HEIGHT));
    };

    mLayoutFunc = [&](IGraphics* pGraphics) {
        // High level GUI setup
        pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
        pGraphics->AttachPanelBackground(COLOR_GRAY);
        pGraphics->EnableMouseOver(true);
        pGraphics->EnableMultiTouch(true);
        pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

        // Setup the panels
        const IRECT bndPadded = pGraphics->GetBounds().GetPadded(-10.0f);
        const IRECT bndSamplePanel = bndPadded.GetFromTop(80).GetReducedFromRight(40);
        const IRECT bndTrackPanel = bndPadded.GetReducedFromTop(90).GetFromTop(80).GetReducedFromRight(40);
        const IRECT bndEnvelopePanel = bndPadded.GetReducedFromTop(180).GetFromTop(250);

        pGraphics->AttachControl(new IVGroupControl(bndSamplePanel, "Sample"));
        pGraphics->AttachControl(new IVGroupControl(bndTrackPanel, "Track"));
        pGraphics->AttachControl(new IVGroupControl(bndEnvelopePanel, "Envelope"));

        // Add the test keyboard and pitch bend wheel
        const IRECT bndKeyboardPanel = bndPadded.GetFromBottom(200);
        const IRECT bndKeyboard = bndKeyboardPanel.GetReducedFromLeft(60.0f);
        const IRECT bndPitchWheel = bndKeyboardPanel.GetFromLeft(50.0f);

        pGraphics->AttachControl(new IWheelControl(bndPitchWheel), kCtrlTagBender);
        pGraphics->AttachControl(new IVKeyboardControl(bndKeyboard), kCtrlTagKeyboard);

        #if false
        pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(0, 2, 6).GetCentredInside(90), kParamGain, "Gain"));
        pGraphics->AttachControl(new IVKnobControl(controls.GetGridCell(1, 2, 6).GetCentredInside(90), kParamNoteGlideTime, "Glide"));

        const IRECT sliders = controls.GetGridCell(2, 2, 6).Union(controls.GetGridCell(3, 2, 6)).Union(controls.GetGridCell(4, 2, 6));
        pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(0, 1, 4).GetMidHPadded(30.), kParamAttack, "Attack"));
        pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(1, 1, 4).GetMidHPadded(30.), kParamDecay, "Decay"));
        pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(2, 1, 4).GetMidHPadded(30.), kParamSustain, "Sustain"));
        pGraphics->AttachControl(new IVSliderControl(sliders.GetGridCell(3, 1, 4).GetMidHPadded(30.), kParamRelease, "Release"));
        #endif

        // Add the volume meter
        const IRECT bndVolMeter = bndPadded.GetReducedFromTop(10).GetFromRight(30).GetFromTop(160);
        pGraphics->AttachControl(new IVLEDMeterControl<2>(bndVolMeter), kCtrlTagMeter);

        #if false
        pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 0, 2, 3).GetCentredInside(60), kParamLFORateHz, "Rate"), kNoTag, "LFO")->Hide(true);
        pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 0, 2, 3).GetCentredInside(60), kParamLFORateTempo, "Rate"), kNoTag, "LFO")->DisablePrompt(false);
        pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 1, 2, 3).GetCentredInside(60), kParamLFODepth, "Depth"), kNoTag, "LFO");
        pGraphics->AttachControl(new IVKnobControl(lfoPanel.GetGridCell(0, 2, 2, 3).GetCentredInside(60), kParamLFOShape, "Shape"), kNoTag, "LFO")->DisablePrompt(false);

        pGraphics->AttachControl(
            new IVSlideSwitchControl(
                lfoPanel.GetGridCell(1, 0, 2, 3).GetFromTop(30).GetMidHPadded(20),
                kParamLFORateMode,
                "Sync",
                DEFAULT_STYLE.WithShowValue(false).WithShowLabel(false).WithWidgetFrac(0.5f).WithDrawShadows(false),
                false
            ),
            kNoTag,
            "LFO"
        )->SetAnimationEndActionFunction(
            [pGraphics](IControl* pControl) noexcept {
                bool sync = pControl->GetValue() > 0.5;
                pGraphics->HideControl(kParamLFORateHz, sync);
                pGraphics->HideControl(kParamLFORateTempo, !sync);
            }
        );

        pGraphics->AttachControl(new IVGroupControl("LFO", "LFO", 10.f, 20.f, 10.f, 10.f));
        #endif

        pGraphics->SetQwertyMidiKeyHandlerFunc(
            [pGraphics](const IMidiMsg& msg) noexcept {
                dynamic_cast<IVKeyboardControl*>(pGraphics->GetControlWithTag(kCtrlTagKeyboard))->SetNoteFromMidi(msg.NoteNumber(), msg.StatusMsg() == IMidiMsg::kNoteOn);
            }
        );
    };
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Setup DSP related stuff
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::DoDspSetup() noexcept {
    // Create the PlayStation SPU core
    Spu::initCore(mSpu, kSpuRamSize, kSpuNumVoices);

    // Set default volume levels
    mSpu.masterVol.left = 0x3FFF;
    mSpu.masterVol.right = 0x3FFF;
    mSpu.reverbVol.left = 0;
    mSpu.reverbVol.right = 0;
    mSpu.extInputVol.left = 0;
    mSpu.extInputVol.right = 0;

    // Setup other SPU settings
    mSpu.bUnmute = true;
    mSpu.bReverbWriteEnable = false;
    mSpu.bExtEnabled = false;
    mSpu.bExtReverbEnable = false;
    mSpu.pExtInputCallback = nullptr;
    mSpu.pExtInputUserData = nullptr;
    mSpu.cycleCount = 0;
    mSpu.reverbBaseAddr8 = (kSpuRamSize / 8) - 1;   // Allocate no RAM for reverb: this instrument does not use the PSX reverb effects
    mSpu.reverbCurAddr = 0;
    mSpu.processedReverb = {};
    mSpu.reverbRegs = {};

    // Terminate the current sample in SPU RAM
    AddSampleTerminator();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Called when a parameter changes
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::InformHostOfParamChange([[maybe_unused]] int idx, [[maybe_unused]] double normalizedValue) noexcept {
    UpdateSpuVoicesFromParams();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Called when a preset changes
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::OnRestoreState() noexcept {
    // TODO...
    Plugin::OnRestoreState();
    UpdateSpuVoicesFromParams();
    AddSampleTerminator();
}

void PsxSampler::UpdateSpuVoicesFromParams() noexcept {
    // TODO...
    std::lock_guard<std::recursive_mutex> lockSpu(mSpuMutex);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Add a terminator for the currently loaded sample consisting of two silent ADPCM blocks which will loop indefinitely.
// Used to guarantee a sound will stop playing after it reaches the end, since SPU voices technically never stop.
// The SPU emulation however will kill them to save on CPU time...
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::AddSampleTerminator() noexcept {
    // Figure out which ADPCM sample block to write the terminators
    constexpr uint32_t kMaxSampleBlocks = kSpuRamSize / Spu::ADPCM_BLOCK_SIZE;
    static_assert(kMaxSampleBlocks >= 2);
    const uint32_t termAdpcmBlocksStartIdx = std::min(mNumSampleBlocks, kMaxSampleBlocks - 2);
    std::byte* const pTermAdpcmBlocks = mSpu.pRam + (size_t) Spu::ADPCM_BLOCK_SIZE * termAdpcmBlocksStartIdx;

    // Zero the bytes for the two ADPCM sample blocks firstly
    std::memset(pTermAdpcmBlocks, 0, Spu::ADPCM_BLOCK_SIZE * 2);

    // The 2nd byte of each ADPCM block is the flags byte, and is where we indicate loop start/end.
    // Make the first block be the loop start, and the second block be loop end:
    pTermAdpcmBlocks[1]   = (std::byte) Spu::ADPCM_FLAG_LOOP_START;
    pTermAdpcmBlocks[17]  = (std::byte) Spu::ADPCM_FLAG_LOOP_END;
}
