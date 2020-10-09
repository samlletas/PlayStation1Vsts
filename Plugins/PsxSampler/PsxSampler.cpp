#include "PsxSampler.h"

#include "../PluginsCommon/VagUtils.h"
#include "IPlug_include_in_plug_src.h"

using namespace AudioTools;

static constexpr uint32_t   kSpuRamSize         = 512 * 1024;   // SPU RAM size: this is the size that the PS1 had
static constexpr int        kNumPresets         = 1;            // Not doing any actual presets for this instrument
static constexpr uint32_t   MIDI_MIDDLE_NOTE    = 60;           // The middle note in MIDI, typically assigned to 'Middle C' in some octave
static constexpr int32_t    PITCH_BEND_CENTER   = 0x2000u;      // Pitch bend center value
static constexpr int32_t    PITCH_BEND_MAX      = 0x3FFFu;      // Maximum pitch bend value

//------------------------------------------------------------------------------------------------------------------------------------------
// Figures out the sample rate of a given note (specified in semitones) using a reference base note (in semitones) and the sample
// rate that the base note sounds at. This is similar to a utility implemented for PsyDoom in the LIBSPU module.
//
// For a good explantion of the conversion from note to frequency, see:
//  https://www.translatorscafe.com/unit-converter/en-US/calculator/note-frequency/
//------------------------------------------------------------------------------------------------------------------------------------------
static float GetNoteSampleRate(const float baseNote, const float baseNoteSampleRate, const float note) noexcept {
    const float noteOffset = note - baseNote;
    const float sampleRate = baseNoteSampleRate * std::powf(2.0f, noteOffset / 12.0f);
    return sampleRate;
}

static double GetNoteSampleRate(const double baseNote, const double baseNoteSampleRate, const double note) noexcept {
    const double noteOffset = note - baseNote;
    const double sampleRate = baseNoteSampleRate * std::pow(2.0, noteOffset / 12.0);
    return sampleRate;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Convert a sample in 16-bit format to a floating point sample
//------------------------------------------------------------------------------------------------------------------------------------------
static double sampleInt16ToDouble(const int16_t origSample) noexcept {
    return (origSample < 0) ? -double(origSample) / INT16_MIN : double(origSample) / INT16_MAX;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Initializes the sampler instrument plugin
//------------------------------------------------------------------------------------------------------------------------------------------
PsxSampler::PsxSampler(const InstanceInfo& info) noexcept
    : Plugin(info, MakeConfig(kNumParams, kNumPresets))
    , mSpu()
    , mSpuMutex()
    , mCurMidiPitchBend(PITCH_BEND_CENTER)
    , mVoiceInfos{}
    , mMeterSender()
    , mMidiQueue()
{
    DefinePluginParams();
    DoDspSetup();
    DoEditorSetup();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Does the main sound processing work of the sampler instrument
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::ProcessBlock(sample** pInputs, sample** pOutputs, int numFrames) noexcept {
    // Process the requested number of samples on the SPU
    const int numChannels = NOutChansConnected();

    {
        std::lock_guard<std::recursive_mutex> lockSpu(mSpuMutex);

        for (int frameIdx = 0; frameIdx < numFrames; frameIdx++) {
            // Process any incoming MIDI messages
            ProcessMidiQueue();

            // Run the SPU and grab the output sample and save
            const Spu::StereoSample soundOut = Spu::stepCore(mSpu);

            if (numChannels >= 2) {
                pOutputs[0][frameIdx] = sampleInt16ToDouble(soundOut.left);
                pOutputs[1][frameIdx] = sampleInt16ToDouble(soundOut.right);
            } else if (numChannels == 1) {
                pOutputs[0][frameIdx] = sampleInt16ToDouble(soundOut.left);
            }
        }
    }

    // Voice management: update the number of samples certain voices are active for and reset the parameters for other voices.
    // Could to this for each sample processed, but that is probably overkill...
    for (uint32_t i = 0; i < kMaxVoices; ++i) {
        VoiceInfo& voiceInfo = mVoiceInfos[i];
        
        if (mSpu.pVoices[i].envPhase != Spu::EnvPhase::Off) {
            voiceInfo.numSamplesActive += (uint32_t) numFrames;
        } else {
            voiceInfo.midiNote = 0xFFFFu;
            voiceInfo.midiVelocity = 0xFFFFu;
            voiceInfo.numSamplesActive = 0;
        }
    }

    // Send the output to the meter
    mMeterSender.ProcessBlock(pOutputs, numFrames, kCtrlTagMeter);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Called periodically to do GUI updates
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::OnIdle() noexcept {
    mMeterSender.TransmitData(*this);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Handle a MIDI message: adds it to the queue to be processed later
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::ProcessMidiMsg(const IMidiMsg& msg) noexcept {
    mMidiQueue.Add(msg);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Defines the parameters used by the plugin
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::DefinePluginParams() noexcept {
    GetParam(kParamSampleRate)->InitInt("sampleRate", 11025, 1, INT32_MAX, "", IParam::EFlags::kFlagMeta);          // Influences 'baseNote'
    GetParam(kParamBaseNote)->InitDouble("baseNote", 84, 0.00001, 10000.0, 0.125, "", IParam::EFlags::kFlagMeta);   // Influences 'sampleRate'
    GetParam(kParamLengthInSamples)->InitInt("lengthInSamples", 0, 0, INT32_MAX);
    GetParam(kParamLengthInBlocks)->InitInt("lengthInBlocks", 0, 0, INT32_MAX);
    GetParam(kParamLoopStartSample)->InitInt("loopStartSample", 0, 0, INT32_MAX);
    GetParam(kParamLoopEndSample)->InitInt("loopEndSample", 0, 0, INT32_MAX);
    GetParam(kParamVolume)->InitInt("volume", 127, 0, 127);
    GetParam(kParamPan)->InitInt("pan", 64, 0, 127);
    GetParam(kParamPitchstepUp)->InitInt("pitchstepUp", 1, 0, 36);
    GetParam(kParamPitchstepDown)->InitInt("pitchstepDown", 1, 0, 36);
    GetParam(kParamAttackStep)->InitInt("attackStep", 3, 0, 3);
    GetParam(kParamAttackShift)->InitInt("attackShift", 0, 0, 31);
    GetParam(kParamAttackIsExp)->InitInt("attackIsExp", 0, 0, 1);
    GetParam(kParamDecayShift)->InitInt("decayShift", 0, 0, 15);
    GetParam(kParamSustainLevel)->InitInt("sustainLevel", 15, 0, 15);
    GetParam(kParamSustainStep)->InitInt("sustainStep", 0, 0, 3);
    GetParam(kParamSustainShift)->InitInt("sustainShift", 31, 0, 31);
    GetParam(kParamSustainDec)->InitInt("sustainDec", 0, 0, 1);
    GetParam(kParamSustainIsExp)->InitInt("sustainIsExp", 1, 0, 1);
    GetParam(kParamReleaseShift)->InitInt("releaseShift", 0, 0, 31);
    GetParam(kParamReleaseIsExp)->InitInt("releaseIsExp", 0, 0, 1);
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

        // Styles
        const IVStyle labelStyle =
            DEFAULT_STYLE
            .WithDrawFrame(false)
            .WithDrawShadows(false)
            .WithValueText(
                DEFAULT_TEXT
                .WithVAlign(EVAlign::Middle)
                .WithAlign(EAlign::Near)
                .WithSize(18.0f)
            );
        
        const IText editBoxTextStyle = DEFAULT_TEXT;
        const IColor editBoxBgColor = IColor(255, 255, 255, 255);

        // Setup the panels
        const IRECT bndPadded = pGraphics->GetBounds().GetPadded(-10.0f);
        const IRECT bndSamplePanel = bndPadded.GetFromTop(80).GetFromLeft(300);
        const IRECT bndSampleInfoPanel = bndPadded.GetFromTop(80).GetReducedFromLeft(310).GetFromLeft(510);
        const IRECT bndTrackPanel = bndPadded.GetReducedFromTop(90).GetFromTop(100).GetFromLeft(410);
        const IRECT bndEnvelopePanel = bndPadded.GetReducedFromTop(200).GetFromTop(230).GetFromLeft(860);

        pGraphics->AttachControl(new IVGroupControl(bndSamplePanel, "Sample"));
        pGraphics->AttachControl(new IVGroupControl(bndSampleInfoPanel, "Sample Info"));
        pGraphics->AttachControl(new IVGroupControl(bndTrackPanel, "Track"));
        pGraphics->AttachControl(new IVGroupControl(bndEnvelopePanel, "Envelope"));

        // Make a read only edit box
        const auto makeReadOnlyEditBox = [=](const IRECT bounds, const int paramIdx) noexcept {
            const IText textStyle =
                editBoxTextStyle
                .WithFGColor(IColor(255, 255, 255, 255))
                .WithSize(20.0f)
                .WithAlign(EAlign::Near);

            ICaptionControl* const pCtrl = new ICaptionControl(bounds, paramIdx, textStyle, IColor(0, 0, 0, 0), false);
            pCtrl->SetDisabled(true);
            pCtrl->DisablePrompt(true);
            pCtrl->SetBlend(IBlend(EBlend::Default, 1.0f));
            return pCtrl;
        };

        // Sample panel
        {
            const IRECT bndPanelPadded = bndSamplePanel.GetReducedFromTop(20.0f);
            const IRECT bndColLoadSave = bndPanelPadded.GetFromLeft(100.0f);
            const IRECT bndColRateNoteLabels = bndPanelPadded.GetReducedFromLeft(110.0f).GetFromLeft(100.0f);
            const IRECT bndColRateNoteValues = bndPanelPadded.GetReducedFromLeft(210.0f).GetFromLeft(80.0f).GetPadded(-4.0f);
            
            pGraphics->AttachControl(new IVButtonControl(bndColLoadSave.GetFromTop(30.0f), SplashClickActionFunc, "Save"));
            pGraphics->AttachControl(
                new IVButtonControl(
                    bndColLoadSave.GetFromBottom(30.0f),
                    [=](IControl* const pControl) noexcept {
                        SplashClickActionFunc(pControl);
                        DoLoadVagFilePrompt(*pGraphics);
                    },
                    "Load"
                )
            );

            pGraphics->AttachControl(new IVLabelControl(bndColRateNoteLabels.GetFromTop(30.0f), "Sample Rate", labelStyle));
            pGraphics->AttachControl(new IVLabelControl(bndColRateNoteLabels.GetFromBottom(30.0f), "Base Note", labelStyle));
            pGraphics->AttachControl(new ICaptionControl(bndColRateNoteValues.GetFromTop(20.0f), kParamSampleRate, editBoxTextStyle, editBoxBgColor, false));
            pGraphics->AttachControl(new ICaptionControl(bndColRateNoteValues.GetFromBottom(20.0f), kParamBaseNote, editBoxTextStyle, editBoxBgColor, false));
        }

        // Sample info panel
        {
            const IRECT bndPanelPadded = bndSampleInfoPanel.GetReducedFromTop(20.0f);
            const IRECT bndColLengthLabels = bndPanelPadded.GetReducedFromLeft(10.0f).GetFromLeft(130.0f);
            const IRECT bndColLengthValues = bndPanelPadded.GetReducedFromLeft(150.0f).GetFromLeft(100).GetPadded(-4.0f);
            const IRECT bndColLoopLabels = bndPanelPadded.GetReducedFromLeft(260.0f).GetFromLeft(130.0f);
            const IRECT bndColLoopValues = bndPanelPadded.GetReducedFromLeft(400.0f).GetFromLeft(100.0f).GetPadded(-4.0f);

            pGraphics->AttachControl(new IVLabelControl(bndColLengthLabels.GetFromTop(30.0f), "Length (samples)", labelStyle));
            pGraphics->AttachControl(new IVLabelControl(bndColLengthLabels.GetFromBottom(30.0f), "Length (blocks)", labelStyle));
            pGraphics->AttachControl(makeReadOnlyEditBox(bndColLengthValues.GetFromTop(20.0f), kParamLengthInSamples));
            pGraphics->AttachControl(makeReadOnlyEditBox(bndColLengthValues.GetFromBottom(20.0f), kParamLengthInBlocks));
            pGraphics->AttachControl(new IVLabelControl(bndColLoopLabels.GetFromTop(30.0f), "Loop Start Sample", labelStyle));
            pGraphics->AttachControl(new IVLabelControl(bndColLoopLabels.GetFromBottom(30.0f), "Loop End Sample", labelStyle));
            pGraphics->AttachControl(makeReadOnlyEditBox(bndColLoopValues.GetFromTop(20.0f), kParamLoopStartSample));
            pGraphics->AttachControl(makeReadOnlyEditBox(bndColLoopValues.GetFromBottom(20.0f), kParamLoopEndSample));
        }

        // Track Panel
        {
            const IRECT bndPanelPadded = bndTrackPanel.GetReducedFromTop(24.0f).GetReducedFromBottom(4.0f);
            const IRECT bndColVol = bndPanelPadded.GetFromLeft(80.0f);
            const IRECT bndColPan = bndPanelPadded.GetReducedFromLeft(80.0f).GetFromLeft(80.0f);
            const IRECT bndColPStepUp = bndPanelPadded.GetReducedFromLeft(160.0f).GetFromLeft(120.0f);
            const IRECT bndColPStepDown = bndPanelPadded.GetReducedFromLeft(280.0f).GetFromLeft(120.0f);

            pGraphics->AttachControl(new IVKnobControl(bndColVol, kParamVolume, "Volume", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndColPan, kParamPan, "Pan", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndColPStepUp, kParamPitchstepUp, "Pitchstep Up", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndColPStepDown, kParamPitchstepDown, "Pitchstep Down", DEFAULT_STYLE, true));
        }

        // Envelope Panel
        {
            const IRECT bndPanelPadded = bndEnvelopePanel.GetReducedFromTop(30.0f).GetReducedFromBottom(4.0f);
            const IRECT bndCol1 = bndPanelPadded.GetFromLeft(120.0f);
            const IRECT bndCol2 = bndPanelPadded.GetReducedFromLeft(120.0f).GetFromLeft(120.0f);
            const IRECT bndCol3 = bndPanelPadded.GetReducedFromLeft(240.0f).GetFromLeft(120.0f);
            const IRECT bndCol4 = bndPanelPadded.GetReducedFromLeft(360.0f).GetFromLeft(120.0f);
            const IRECT bndCol5 = bndPanelPadded.GetReducedFromLeft(480.0f).GetFromLeft(120.0f);
            const IRECT bndCol6 = bndPanelPadded.GetReducedFromLeft(600.0f).GetFromLeft(120.0f);
            const IRECT bndCol7 = bndPanelPadded.GetReducedFromLeft(720.0f).GetFromLeft(120.0f);

            pGraphics->AttachControl(new IVKnobControl(bndCol1.GetFromTop(80.0f), kParamAttackStep, "Attack Step", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndCol1.GetReducedFromTop(100.0f).GetFromTop(80.0f), kParamAttackShift, "Attack Shift", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndCol2.GetFromTop(80.0f), kParamAttackIsExp, "Attack Is Exp.", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndCol3.GetFromTop(80.0f), kParamDecayShift, "Decay Shift", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndCol4.GetFromTop(80.0f), kParamSustainLevel, "Sustain Level", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndCol5.GetFromTop(80.0f), kParamSustainStep, "Sustain Step", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndCol5.GetReducedFromTop(100.0f).GetFromTop(80.0f), kParamSustainShift, "Sustain Shift", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndCol6.GetFromTop(80.0f), kParamSustainDec, "Sustain Dec.", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndCol6.GetReducedFromTop(100.0f).GetFromTop(80.0f), kParamSustainIsExp, "Sustain Is Exp.", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndCol7.GetFromTop(80.0f), kParamReleaseShift, "Release Shift", DEFAULT_STYLE, true));
            pGraphics->AttachControl(new IVKnobControl(bndCol7.GetReducedFromTop(100.0f).GetFromTop(80.0f), kParamReleaseIsExp, "Release Is Exp.", DEFAULT_STYLE, true));
        }

        // Add the test keyboard and pitch bend wheel
        const IRECT bndKeyboardPanel = bndPadded.GetFromBottom(200);
        const IRECT bndKeyboard = bndKeyboardPanel.GetReducedFromLeft(60.0f);
        const IRECT bndPitchWheel = bndKeyboardPanel.GetFromLeft(50.0f);

        pGraphics->AttachControl(new IWheelControl(bndPitchWheel), kCtrlTagBender);
        pGraphics->AttachControl(new IVKeyboardControl(bndKeyboard), kCtrlTagKeyboard);

        // Add the volume meter
        const IRECT bndVolMeter = bndPadded.GetReducedFromTop(10).GetFromRight(30).GetFromTop(180);
        pGraphics->AttachControl(new IVLEDMeterControl<2>(bndVolMeter), kCtrlTagMeter);

        // Allow Qwerty keyboard - but only in standalone mode.
        // In VST mode the host might have it's own keyboard input functionality, and this could interfere...
        #if APP_API
          pGraphics->SetQwertyMidiKeyHandlerFunc(
              [pGraphics](const IMidiMsg& msg) noexcept {
                  dynamic_cast<IVKeyboardControl*>(pGraphics->GetControlWithTag(kCtrlTagKeyboard))->SetNoteFromMidi(msg.NoteNumber(), msg.StatusMsg() == IMidiMsg::kNoteOn);
              }
          );
        #endif
    };
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Setup DSP related stuff
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::DoDspSetup() noexcept {
    // Create the PlayStation SPU core
    Spu::initCore(mSpu, kSpuRamSize, kMaxVoices);

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

    // Default initialize all the SPU voice infos
    for (VoiceInfo& voiceInfo : mVoiceInfos) {
        voiceInfo.midiNote = 0xFFFFu;
        voiceInfo.midiVelocity = 0xFFFFu;
        voiceInfo.numSamplesActive = 0;
    }

    // Update SPU voices from the current instrument settings and terminate the current empty sample in SPU RAM
    UpdateSpuVoicesFromParams();
    AddSampleTerminator();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Called when a parameter changes
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::InformHostOfParamChange(int idx, [[maybe_unused]] double normalizedValue) noexcept {
    // These two parameters are linked
    if (idx == kParamSampleRate) {
        SetBaseNoteFromSampleRate();
        GetUI()->SetAllControlsDirty();
    } else if (idx == kParamBaseNote) {
        SetSampleRateFromBaseNote();
        GetUI()->SetAllControlsDirty();
    }

    // Update the SPU voices etc.
    std::lock_guard<std::recursive_mutex> lockSpu(mSpuMutex);
    UpdateSpuVoicesFromParams();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Called when a preset changes
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::OnRestoreState() noexcept {
    // Base plugin restore functionality
    Plugin::OnRestoreState();

    // Update the SPU from the changes and make sure the current sample is terminated
    std::lock_guard<std::recursive_mutex> lockSpu(mSpuMutex);
    UpdateSpuVoicesFromParams();
    AddSampleTerminator();
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
    const uint32_t numSampleBlocks = (uint32_t) GetParam(kParamLengthInBlocks)->Value();
    const uint32_t termAdpcmBlocksStartIdx = std::min(numSampleBlocks, kMaxSampleBlocks - 2);
    std::byte* const pTermAdpcmBlocks = mSpu.pRam + (size_t) Spu::ADPCM_BLOCK_SIZE * termAdpcmBlocksStartIdx;

    // Zero the bytes for the two ADPCM sample blocks firstly
    std::memset(pTermAdpcmBlocks, 0, Spu::ADPCM_BLOCK_SIZE * 2);

    // The 2nd byte of each ADPCM block is the flags byte, and is where we indicate loop start/end.
    // Make the first block be the loop start, and the second block be loop end:
    pTermAdpcmBlocks[1]   = (std::byte) Spu::ADPCM_FLAG_LOOP_START;
    pTermAdpcmBlocks[17]  = (std::byte) Spu::ADPCM_FLAG_LOOP_END;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Process the MIDI queue - advances time by a single sample
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::ProcessMidiQueue() noexcept {
    // Continue processing the queue
    while (!mMidiQueue.Empty()) {
        // Is there a delay until the next message?
        // If so then decrement the time until it and finish up.
        {
            IMidiMsg& msg = mMidiQueue.Peek();

            if (msg.mOffset > 0) {
                msg.mOffset--;
                break;
            }
        }

        // Remove the message from the queue then process
        const IMidiMsg msg = mMidiQueue.Peek();
        mMidiQueue.Remove();
        ProcessQueuedMidiMsg(msg);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Process the given MIDI message that was queued
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::ProcessQueuedMidiMsg(const IMidiMsg& msg) noexcept {
    // What type of message is it?
    const IMidiMsg::EStatusMsg statusMsgType = msg.StatusMsg();
    
    switch (statusMsgType) {
        case IMidiMsg::kNoteOn:
            ProcessMidiNoteOn(msg.mData1 & uint8_t(0x7Fu), msg.mData1 & uint8_t(0x7Fu));
            break;

        case IMidiMsg::kNoteOff:
            ProcessMidiNoteOff(msg.mData1 & uint8_t(0x7Fu));
            break;

        case IMidiMsg::kControlChange: {
            switch (msg.mData1) {
                case IMidiMsg::kChannelVolume:
                    ProcessMidiVolume(msg.mData2 & uint8_t(0x7Fu));
                    break;

                case IMidiMsg::kPan:
                    ProcessMidiPan(msg.mData2 & uint8_t(0x7Fu));
                    break;

                default:
                    break;
            }
        };

        case IMidiMsg::kPitchWheel:
            ProcessMidiPitchBend((uint16_t)((((uint16_t) msg.mData1 & 0x7Fu) << 7) | ((uint16_t) msg.mData2 & 0x7Fu)));
            break;
        
        default:
            break;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Handle a MIDI note on message
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::ProcessMidiNoteOn(const uint8_t note, const uint8_t velocity) noexcept {
    // Make sure this note is not already playing - just allowed to have 1 instance
    for (uint32_t i = 0; i < kMaxVoices; ++i) {
        if (mVoiceInfos[i].midiNote == note)
            return;
    }

    // Try to find a free SPU voice firstly to service this request
    uint32_t spuVoiceIdx = UINT32_MAX;

    for (uint32_t i = 0; i < kMaxVoices; ++i) {
        if (mSpu.pVoices[i].envPhase == Spu::EnvPhase::Off) {
            spuVoiceIdx = i;
            break;
        }
    }

    // If that fails try to find the oldest playing voice to use
    if (spuVoiceIdx == UINT32_MAX) {
        spuVoiceIdx = 0;
        uint32_t oldestSamplesActive = mVoiceInfos[0].numSamplesActive;

        for (uint32_t i = 1; i < kMaxVoices; ++i) {
            if (mVoiceInfos[i].numSamplesActive > oldestSamplesActive) {
                oldestSamplesActive = mVoiceInfos[i].numSamplesActive;
                spuVoiceIdx = i;
            }
        }
    }

    // Save the info for this voice
    VoiceInfo& voiceInfo = mVoiceInfos[spuVoiceIdx];
    voiceInfo.midiNote = note;
    voiceInfo.midiVelocity = velocity;
    voiceInfo.numSamplesActive = 0;

    // Make sure the voice parameters are up to date and sound the voice
    UpdateSpuVoiceFromParams(spuVoiceIdx);
    Spu::keyOn(mSpu.pVoices[spuVoiceIdx]);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Handle a MIDI note off message
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::ProcessMidiNoteOff(const uint8_t note) noexcept {
    // Try to find which voice for this note
    for (uint32_t i = 0; i < kMaxVoices; ++i) {
        if (mVoiceInfos[i].midiNote == note) {
            // Found the voice: make sure it is in the correct envelope phase to be killed off
            Spu::Voice& voice = mSpu.pVoices[i];

            if ((voice.envPhase != Spu::EnvPhase::Release) && (voice.envPhase != Spu::EnvPhase::Off)) {
                Spu::keyOff(voice);
            }

            // Done searching
            break;
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Handle a MIDI channel volume control message
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::ProcessMidiVolume(const uint8_t volume) noexcept {
    GetParam(kParamVolume)->Set(volume);
    GetUI()->SetAllControlsDirty();
    UpdateSpuVoicesFromParams();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Handle a MIDI pan control message
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::ProcessMidiPan(const uint8_t pan) noexcept {
    GetParam(kParamPan)->Set(pan);
    GetUI()->SetAllControlsDirty();
    UpdateSpuVoicesFromParams();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Handle a MIDI pitch bend message
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::ProcessMidiPitchBend(const uint16_t pitchBend) noexcept {
    mCurMidiPitchBend = pitchBend;
    UpdateSpuVoicesFromParams();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Update all of the SPU voices from the current parameters.
// Note: assumes the SPU lock is held.
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::UpdateSpuVoicesFromParams() noexcept {
    // These parameters affect the pitch and volume of all voices
    const uint32_t sampleRate = (uint32_t) GetParam(kParamSampleRate)->Value();
    const uint32_t volume = (uint32_t) GetParam(kParamVolume)->Value();
    const uint32_t pan = (uint32_t) GetParam(kParamPan)->Value();

    // Get the current pitch bend to apply to all voices (in semitones) and the ADSR envelope to use for all voices
    Spu::AdsrEnvelope adsrEnv = GetCurrentSpuAdsrEnv();
    const float pitchBendInNotes = GetCurrentPitchBendInNotes();

    // Update all the voices
    const uint32_t numVoices = mSpu.numVoices;
    Spu::Voice* const pVoices = mSpu.pVoices;

    for (uint32_t voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx) {
        const VoiceInfo& voiceInfo = mVoiceInfos[voiceIdx];
        Spu::Voice& voice = pVoices[voiceIdx];

        voice.sampleRate = CalcSpuVoiceSampleRate(sampleRate, (float) voiceInfo.midiNote + pitchBendInNotes);
        voice.bDisabled = false;
        voice.bDoReverb = false;
        voice.env = adsrEnv;
        voice.volume = CalcSpuVoiceVolume(volume, pan, voiceInfo.midiVelocity);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Update a single SPU voice (only) from current parameters.
// Note: assumes the SPU lock is held.
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::UpdateSpuVoiceFromParams(const uint32_t voiceIdx) noexcept {
    assert(voiceIdx < kMaxVoices);

    // This will affect the pitch and volume of the voice
    const uint32_t sampleRate = (uint32_t) GetParam(kParamSampleRate)->Value();
    const uint32_t volume = (uint32_t) GetParam(kParamVolume)->Value();
    const uint32_t pan = (uint32_t) GetParam(kParamPan)->Value();

    // Get the envelope to use and the current pitch bend
    Spu::AdsrEnvelope adsrEnv = GetCurrentSpuAdsrEnv();
    const float pitchBendInNotes = GetCurrentPitchBendInNotes();

    // Update the voice
    const VoiceInfo& voiceInfo = mVoiceInfos[voiceIdx];
    Spu::Voice& voice = mSpu.pVoices[voiceIdx];

    voice.sampleRate = CalcSpuVoiceSampleRate(sampleRate, (float) voiceInfo.midiNote + pitchBendInNotes);
    voice.bDisabled = false;
    voice.bDoReverb = false;
    voice.env = adsrEnv;
    voice.volume = CalcSpuVoiceVolume(volume, pan, voiceInfo.midiVelocity);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Compute the SPU's format for sample rate where a value of 0x1000 means 44,100 Hz, half that is 22,050 Hz and so on.
// Computes using the sample rate of the sound (11,025 etc.) and the current note being played, which can also be fractional too.
//------------------------------------------------------------------------------------------------------------------------------------------
uint16_t PsxSampler::CalcSpuVoiceSampleRate(const uint32_t baseSampleRate, const float voiceNote) noexcept {
    // Convert the base sample rate into the SPU's scale.
    // For the SPU a sample rate of '11025' is equal to '0x400' so compute how many '11025' units we have and then scale by '0x400' (1024)
    const float baseSampleRateSpu = ((float) baseSampleRate / 11025.0f) * 1024.0f;

    // Now figure out the SPU rate to play the note at, round and clamp
    const float spuSampleRate = GetNoteSampleRate((float) MIDI_MIDDLE_NOTE, baseSampleRateSpu, voiceNote);
    return (uint16_t) std::clamp<float>(std::roundf(spuSampleRate), 0.0f, UINT16_MAX);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Compute the left/right volume for an SPU voice given the instrument volume (0-127) and pan (0-127, 64 = center) and the velocity
// that the note was sounded with, from 0-127.
//------------------------------------------------------------------------------------------------------------------------------------------
Spu::Volume PsxSampler::CalcSpuVoiceVolume(const uint32_t volume, const uint32_t pan, const uint32_t velocity) noexcept {
    const float volumeF = std::max((float) volume / 127.0f, 1.0f);
    const float velocityF = std::max((float) velocity / 127.0f, 1.0f);
    const float scaleF = volumeF * velocityF;
    const float panF = (pan < 64) ? ((float) pan - 64.0f) / 64.0f : std::max(((float) pan - 64.0f) / 63.0f, 1.0f);
    const float volumeLF = (1.0f - panF) / 2.0f;
    const float volumeRF = (1.0f + panF) / 2.0f;

    return Spu::Volume{
        (int16_t) std::round(volumeLF * (float) 0x7FFF),
        (int16_t) std::round(volumeRF * (float) 0x7FFF)
    };
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Get the current SPU ADSR envelope to use, based on the instrument parameters
//------------------------------------------------------------------------------------------------------------------------------------------
Spu::AdsrEnvelope PsxSampler::GetCurrentSpuAdsrEnv() const noexcept {  
    const uint32_t attackStep = (uint32_t) GetParam(kParamAttackStep)->Value();
    const uint32_t attackShift = (uint32_t) GetParam(kParamAttackShift)->Value();
    const uint32_t attackIsExp = (uint32_t) GetParam(kParamAttackIsExp)->Value();
    const uint32_t decayShift = (uint32_t) GetParam(kParamDecayShift)->Value();
    const uint32_t sustainLevel = (uint32_t) GetParam(kParamSustainLevel)->Value();
    const uint32_t sustainStep = (uint32_t) GetParam(kParamSustainStep)->Value();
    const uint32_t sustainShift = (uint32_t) GetParam(kParamSustainShift)->Value();
    const uint32_t sustainDec = (uint32_t) GetParam(kParamSustainDec)->Value();
    const uint32_t sustainIsExp = (uint32_t) GetParam(kParamSustainIsExp)->Value();
    const uint32_t releaseShift = (uint32_t) GetParam(kParamReleaseShift)->Value();
    const uint32_t releaseIsExp = (uint32_t) GetParam(kParamReleaseIsExp)->Value();

    Spu::AdsrEnvelope adsrEnv = {};
    adsrEnv.sustainLevel = sustainLevel;
    adsrEnv.decayShift = decayShift;
    adsrEnv.attackStep = attackStep;
    adsrEnv.attackShift = attackShift;
    adsrEnv.bAttackExp = attackIsExp;
    adsrEnv.releaseShift = releaseShift;
    adsrEnv.bReleaseExp = releaseIsExp;
    adsrEnv.sustainStep = sustainStep;
    adsrEnv.sustainShift = sustainShift;
    adsrEnv.bSustainDec = sustainDec;
    adsrEnv.bSustainExp = sustainIsExp;

    return adsrEnv;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Return how many semitones of pitch bend are currently being applied based on the current MIDI pitch bend value and the pitchbend range
//------------------------------------------------------------------------------------------------------------------------------------------
float PsxSampler::GetCurrentPitchBendInNotes() const noexcept {
    // Get the range of the pitch bend in semitones
    const float pitchstepUp = (float) GetParam(kParamPitchstepUp)->Value();
    const float pitchstepDown = (float) GetParam(kParamPitchstepDown)->Value();

    // Get the clamped MIDI pitch bend
    const uint32_t midiPitchBend = std::min<uint32_t>(mCurMidiPitchBend, PITCH_BEND_MAX);

    // Get the normalized pitch bend in a -1.0 to +1.0 range:
    const float pitchBendNormalized = (midiPitchBend < PITCH_BEND_CENTER) ?
        ((float) mCurMidiPitchBend - (float) PITCH_BEND_CENTER) / (float)(PITCH_BEND_CENTER) :
        ((float) mCurMidiPitchBend - (float) PITCH_BEND_CENTER) / (float)(PITCH_BEND_CENTER - 1);

    // Figure out the semitone pitch bend and return
    return (pitchBendNormalized < 0) ? pitchBendNormalized * pitchstepDown : pitchBendNormalized * pitchstepUp;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Prompt the user to load a sample in .vag file and load it if a choice is made
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::DoLoadVagFilePrompt(IGraphics& graphics) noexcept {
    // Prompt for the file to open and abort if none is chosen
    WDL_String filePath;
    WDL_String fileDir;
    graphics.PromptForFile(filePath, fileDir, EFileAction::Open, "vag");

    if (filePath.GetLength() <= 0)
        return;

    // Read the VAG file
    std::vector<std::byte> adpcmData;
    uint32_t sampleRate = {};
    std::string loadErrorMsg;

    if (!VagUtils::readVagFile(filePath.Get(), adpcmData, sampleRate, loadErrorMsg))
        return;

    // Decode to figure out where the loop points are in the VAG file
    std::vector<int16_t> pcmSamples;
    uint32_t loopStartSample = {};
    uint32_t loopEndSample = {};
    VagUtils::decodePsxAdpcmSamples(adpcmData.data(), (uint32_t) adpcmData.size(), pcmSamples, loopStartSample, loopEndSample);

    // Clamp the length of the VAG file to be within the RAM size of the SPU
    const uint32_t numAdpcmBlocks = std::min((uint32_t) adpcmData.size(), kSpuRamSize) / Spu::ADPCM_BLOCK_SIZE;

    // Transfer the sound data to the SPU, terminate the sample, and update some of the sampler parameters
    std::lock_guard<std::recursive_mutex> lockSpu(mSpuMutex);

    std::memcpy(mSpu.pRam, adpcmData.data(), (size_t) numAdpcmBlocks * Spu::ADPCM_BLOCK_SIZE);

    GetParam(kParamSampleRate)->Set((double) sampleRate);
    SetBaseNoteFromSampleRate();
    GetParam(kParamLengthInSamples)->Set((double) pcmSamples.size());
    GetParam(kParamLengthInBlocks)->Set((double) numAdpcmBlocks);
    GetParam(kParamLoopStartSample)->Set((double) loopStartSample);
    GetParam(kParamLoopEndSample)->Set((double) loopEndSample);
    GetUI()->SetAllControlsDirty();

    AddSampleTerminator();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Set the base note value from the sample rate
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::SetBaseNoteFromSampleRate() noexcept {
    const uint32_t sampleRate = (uint32_t) GetParam(kParamSampleRate)->Value();

    if (sampleRate <= 22050.0) {
        const double octavesDiff = std::log2(22050.0 / (double) sampleRate);
        const double baseNote = 60.0 + octavesDiff * 12.0;
        const double baseNoteRounded = std::round(baseNote * 256.0) / 256.0;    // Round to 1/256 increments
        GetParam(kParamBaseNote)->Set(baseNoteRounded);
    } else {
        const double octavesDiff = std::log2((double) sampleRate / 22050.0);
        const double baseNote = 60.0 - octavesDiff * 12.0;
        const double baseNoteRounded = std::round(baseNote * 256.0) / 256.0;    // Round to 1/256 increments
        GetParam(kParamBaseNote)->Set(baseNoteRounded);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Set the sample rate value from the base note
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSampler::SetSampleRateFromBaseNote() noexcept {
    const double sampleRate = GetNoteSampleRate(GetParam(kParamBaseNote)->Value(), 22050.0, 60.0);
    const double sampleRateRounded = std::round(sampleRate);
    GetParam(kParamSampleRate)->Set(sampleRateRounded);
}
