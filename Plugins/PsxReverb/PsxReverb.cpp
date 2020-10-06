#include "PsxReverb.h"

#include "IControls.h"
#include "IPlug_include_in_plug_src.h"
#include "SpuReverbPresets.h"

static constexpr int        kNumPresets = 10;           // How many reverb presets there are
static constexpr uint32_t   kSpuRamSize = 512 * 1024;   // SPU RAM size: this is the size that the PS1 had

//------------------------------------------------------------------------------------------------------------------------------------------
// Initializes the reverb plugin
//------------------------------------------------------------------------------------------------------------------------------------------
PsxReverb::PsxReverb(const InstanceInfo& info) noexcept
    : Plugin(info, MakeConfig(kNumParams, kNumPresets))
#if IPLUG_DSP
    , mSpu()
    , mSpuMutex()
    , mSpuInputSample()
#endif
{
    DefinePluginParams();
    DefinePluginPresets();
    
    #if IPLUG_DSP
        DoDspSetup();
    #endif

    #if IPLUG_EDITOR
        DoEditorSetup();
    #endif
}

#if IPLUG_DSP

//------------------------------------------------------------------------------------------------------------------------------------------
// Convert a sample in double format to 16-bit
//------------------------------------------------------------------------------------------------------------------------------------------
static int16_t sampleDoubleToInt16(const double origSample) noexcept {
    const double origClamped = std::clamp(origSample, -1.0, 1.0);
    return (origClamped < 0) ? (int16_t)(-origClamped * double(INT16_MIN)) : (int16_t)(origClamped * double(INT16_MAX));
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Convert a sample in 16-bit format to a floating point sample
//------------------------------------------------------------------------------------------------------------------------------------------
static double sampleInt16ToDouble(const int16_t origSample) noexcept {
    return (origSample < 0) ? -double(origSample) / INT16_MIN : double(origSample) / INT16_MAX;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Does the work of the reverb effect plugin
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::ProcessBlock(sample** pInputs, sample** pOutputs, int numFrames) noexcept {
    std::lock_guard<std::recursive_mutex> lockSpu(mSpuMutex);

    // Process the requested number of samples
    const int numChannels = NOutChansConnected();
    
    for (int frameIdx = 0; frameIdx < numFrames; frameIdx++) {
        // Setup the SPU input sample
        if (numChannels >= 2) {
            mSpuInputSample.left = sampleDoubleToInt16(pInputs[0][frameIdx]);
            mSpuInputSample.right = sampleDoubleToInt16(pInputs[1][frameIdx]);
        } else if (numChannels == 1) {
            mSpuInputSample.left = sampleDoubleToInt16(pInputs[0][frameIdx]);
            mSpuInputSample.right = sampleDoubleToInt16(pInputs[0][frameIdx]);
        } else {
            mSpuInputSample = {};
        }

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

#endif  // #if IPLUG_DSP

//------------------------------------------------------------------------------------------------------------------------------------------
// Defines the parameters used by the plugin
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::DefinePluginParams() noexcept {
    GetParam(kMasterVolL)->InitInt("masterVolL", 0, 0, 0x3FFF);
    GetParam(kMasterVolR)->InitInt("masterVolR", 0, 0, 0x3FFF);
    GetParam(kInputVolL)->InitInt("inputVolL", 0, 0, 0x7FFF);
    GetParam(kInputVolR)->InitInt("inputVolR", 0, 0, 0x7FFF);
    GetParam(kReverbVolL)->InitInt("reverbVolL", 0, 0, 0x7FFF);
    GetParam(kReverbVolR)->InitInt("reverbVolR", 0, 0, 0x7FFF);

    GetParam(kWABaseAddr)->InitInt("revBaseAddr", 0, 0, UINT16_MAX);
    GetParam(kVolLIn)->InitInt("volLIn", 0, INT16_MIN, INT16_MAX);
    GetParam(kVolRIn)->InitInt("volRIn", 0, INT16_MIN, INT16_MAX);
    GetParam(kVolIIR)->InitInt("volIIR", 0, INT16_MIN, INT16_MAX);
    GetParam(kVolWall)->InitInt("volWall", 0, INT16_MIN, INT16_MAX);
    GetParam(kVolAPF1)->InitInt("volAPF1", 0, INT16_MIN, INT16_MAX);
    GetParam(kVolAPF2)->InitInt("volAPF2", 0, INT16_MIN, INT16_MAX);
    GetParam(kVolComb1)->InitInt("volComb1", 0, INT16_MIN, INT16_MAX);
    GetParam(kVolComb2)->InitInt("volComb2", 0, INT16_MIN, INT16_MAX);

    GetParam(kVolComb3)->InitInt("volComb3", 0, INT16_MIN, INT16_MAX);
    GetParam(kVolComb4)->InitInt("volComb4", 0, INT16_MIN, INT16_MAX);
    GetParam(kDispAPF1)->InitInt("dispAPF1", 0, 0, UINT16_MAX);
    GetParam(kDispAPF2)->InitInt("dispAPF2", 0, 0, UINT16_MAX);
    GetParam(kAddrLAPF1)->InitInt("addrLAPF1", 0, 0, UINT16_MAX);
    GetParam(kAddrRAPF1)->InitInt("addrRAPF1", 0, 0, UINT16_MAX);
    GetParam(kAddrLAPF2)->InitInt("addrLAPF2", 0, 0, UINT16_MAX);
    GetParam(kAddrRAPF2)->InitInt("addrRAPF2", 0, 0, UINT16_MAX);

    GetParam(kAddrLComb1)->InitInt("addrLComb1", 0, 0, UINT16_MAX);
    GetParam(kAddrRComb1)->InitInt("addrRComb1", 0, 0, UINT16_MAX);
    GetParam(kAddrLComb2)->InitInt("addrLComb2", 0, 0, UINT16_MAX);
    GetParam(kAddrRComb2)->InitInt("addrRComb2", 0, 0, UINT16_MAX);
    GetParam(kAddrLComb3)->InitInt("addrLComb3", 0, 0, UINT16_MAX);
    GetParam(kAddrRComb3)->InitInt("addrRComb3", 0, 0, UINT16_MAX);
    GetParam(kAddrLComb4)->InitInt("addrLComb4", 0, 0, UINT16_MAX);
    GetParam(kAddrRComb4)->InitInt("addrRComb4", 0, 0, UINT16_MAX);

    GetParam(kAddrLSame1)->InitInt("addrLSame1", 0, 0, UINT16_MAX);
    GetParam(kAddrRSame1)->InitInt("addrRSame1", 0, 0, UINT16_MAX);
    GetParam(kAddrLSame2)->InitInt("addrLSame2", 0, 0, UINT16_MAX);
    GetParam(kAddrRSame2)->InitInt("addrRSame2", 0, 0, UINT16_MAX);
    GetParam(kAddrLDiff1)->InitInt("addrLDiff1", 0, 0, UINT16_MAX);
    GetParam(kAddrRDiff1)->InitInt("addrRDiff1", 0, 0, UINT16_MAX);
    GetParam(kAddrLDiff2)->InitInt("addrLDiff2", 0, 0, UINT16_MAX);
    GetParam(kAddrRDiff2)->InitInt("addrRDiff2", 0, 0, UINT16_MAX);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Defines the presets for the effect plugin.
// These are the actual effect presets found in the PsyQ SDK.
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::DefinePluginPresets() noexcept {
    using namespace SpuReverbPresets;

    for (int32_t i = 0; i < SPU_REV_MODE_MAX; ++i) {
        const char* const presetName = gReverbModeNames[i];
        const SpuReverbDef& reverbDef = gReverbDefs[i];
        const uint16_t workAreaBaseAddr = gReverbWorkAreaBaseAddrs[i];

        MakePreset(
            presetName,
            0x3FFFu,                  // masterVolL
            0x3FFFu,                  // masterVolR
            0x7FFFu,                  // inputVolL
            0x7FFFu,                  // inputVolR
            (i != 0) ? 0x2FFFu : 0,   // reverbVolL
            (i != 0) ? 0x2FFFu : 0,   // reverbVolR
            (i != 0) ? workAreaBaseAddr : 0xFFFFu,
            reverbDef.apfOffset1,
            reverbDef.apfOffset2,
            (int16_t) reverbDef.reflectionVolume1,
            (int16_t) reverbDef.combVolume1,
            (int16_t) reverbDef.combVolume2,
            (int16_t) reverbDef.combVolume3,
            (int16_t) reverbDef.combVolume4,
            (int16_t) reverbDef.reflectionVolume2,
            (int16_t) reverbDef.apfVolume1,
            (int16_t) reverbDef.apfVolume2,
            reverbDef.sameSideRefractAddr1Left,
            reverbDef.sameSideRefractAddr1Right,
            reverbDef.combAddr1Left,
            reverbDef.combAddr1Right,
            reverbDef.combAddr2Left,
            reverbDef.combAddr2Right,
            reverbDef.sameSideRefractAddr2Left,
            reverbDef.sameSideRefractAddr2Right,
            reverbDef.diffSideReflectAddr1Left,
            reverbDef.diffSideReflectAddr1Right,
            reverbDef.combAddr3Left,
            reverbDef.combAddr3Right,
            reverbDef.combAddr4Left,
            reverbDef.combAddr4Right,
            reverbDef.diffSideReflectAddr2Left,
            reverbDef.diffSideReflectAddr2Right,
            reverbDef.apfAddr1Left,
            reverbDef.apfAddr1Right,
            reverbDef.apfAddr2Left,
            reverbDef.apfAddr2Right,
            (int16_t) reverbDef.inputVolLeft,
            (int16_t) reverbDef.inputVolRight
        );
    }
}

#if IPLUG_EDITOR

//------------------------------------------------------------------------------------------------------------------------------------------
// Setup controls for the plugin's GUI
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::DoEditorSetup() noexcept {
    mMakeGraphicsFunc = [&]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_HEIGHT));
    };
    
    mLayoutFunc = [&](IGraphics* pGraphics) {
        pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
        pGraphics->AttachPanelBackground(COLOR_GRAY);
        pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

        IVBakedPresetManagerControl* const pPresetMgrCtrl = new IVBakedPresetManagerControl(IRECT(0.0f, 0.0f, 600.0f, 40.0f), DEFAULT_STYLE);
        pGraphics->AttachControl(pPresetMgrCtrl);

        const auto addHSlider = [=](const int ctrlTag, const char* const label, const float x, const float y, const float w, const float h) noexcept {
            igraphics::IVStyle style = DEFAULT_STYLE;
            style.showValue = false;
            style.labelText.mAlign = igraphics::EAlign::Near;
            pGraphics->AttachControl(new IVSliderControl(IRECT(x, y, x + w, y + h), ctrlTag, label, style, false, EDirection::Horizontal));
        };

        const auto addTInput = [=](const int ctrlTag, const float x, const float y, const float w, const float h) noexcept {
            IColor bgColor(255, 255, 255, 255);
            pGraphics->AttachControl(new ICaptionControl(IRECT(x, y, x + w, y + h), ctrlTag, DEFAULT_TEXT, bgColor));
        };

        addHSlider(kReverbVolL, "Reverb L-Vol",   10,   50, 130, 40);   addTInput(kReverbVolL,  145,  70, 45, 20);
        addHSlider(kReverbVolR, "Reverb R-Vol",   10,   90, 130, 40);   addTInput(kReverbVolR,  145, 110, 45, 20);
        addHSlider(kInputVolL,  "Input L-Vol",    205,  50, 130, 40);   addTInput(kInputVolL,   340,  70, 45, 20);
        addHSlider(kInputVolR,  "Input R-Vol",    205,  90, 130, 40);   addTInput(kInputVolR,   340, 110, 45, 20);
        addHSlider(kMasterVolL, "Master L-Vol",   400,  50, 130, 40);   addTInput(kMasterVolL,  535,  70, 45, 20);
        addHSlider(kMasterVolR, "Master R-Vol",   400,  90, 130, 40);   addTInput(kMasterVolR,  535, 110, 45, 20);

        pGraphics->AttachControl(
            new IVButtonControl(
                IRECT(600, 80, 800, 110),
                [this](IControl* pCaller){
                    ClearReverbWorkArea();
                    pCaller->OnEndAnimation();
                },
                "Clear Rev. Work Area",
                DEFAULT_STYLE,
                true,
                false
            )
        );

        pGraphics->AttachControl(new ITextControl(IRECT(0, 150, PLUG_WIDTH, 164), "- Advanced Settings -", DEFAULT_TEXT, COLOR_MID_GRAY));

        addHSlider(kWABaseAddr, "WA Base Addr",    10, 180, 130, 40);   addTInput(kWABaseAddr,  145, 200, 45, 20);
        addHSlider(kVolLIn,     "In L-Vol",        10, 220, 130, 40);   addTInput(kVolLIn,      145, 240, 45, 20);
        addHSlider(kVolRIn,     "In R-Vol",        10, 260, 130, 40);   addTInput(kVolRIn,      145, 280, 45, 20);
        addHSlider(kVolIIR,     "Refl Vol 1",      10, 300, 130, 40);   addTInput(kVolIIR,      145, 320, 45, 20);
        addHSlider(kVolWall,    "Refl Vol 2",      10, 340, 130, 40);   addTInput(kVolWall,     145, 360, 45, 20);
        addHSlider(kVolAPF1,    "APF Vol 1",       10, 380, 130, 40);   addTInput(kVolAPF1,     145, 400, 45, 20);
        addHSlider(kVolAPF2,    "APF Vol 2",       10, 420, 130, 40);   addTInput(kVolAPF2,     145, 440, 45, 20);
        addHSlider(kVolComb1,   "Comb Vol 1",      10, 460, 130, 40);   addTInput(kVolComb1,    145, 480, 45, 20);
        addHSlider(kVolComb2,   "Comb Vol 2",      10, 500, 130, 40);   addTInput(kVolComb2,    145, 520, 45, 20);

        addHSlider(kVolComb3,   "Comb Vol 3",     205, 220, 130, 40);   addTInput(kVolComb3,    340, 240, 45, 20);
        addHSlider(kVolComb4,   "Comb Vol 4",     205, 260, 130, 40);   addTInput(kVolComb4,    340, 280, 45, 20);
        addHSlider(kDispAPF1,   "APF Offset 1",   205, 300, 130, 40);   addTInput(kDispAPF1,    340, 320, 45, 20);
        addHSlider(kDispAPF2,   "APF Offset 2",   205, 340, 130, 40);   addTInput(kDispAPF2,    340, 360, 45, 20);
        addHSlider(kAddrLAPF1,  "APF L-Addr 1",   205, 380, 130, 40);   addTInput(kAddrLAPF1,   340, 400, 45, 20);
        addHSlider(kAddrRAPF1,  "APF R-Addr 1",   205, 420, 130, 40);   addTInput(kAddrRAPF1,   340, 440, 45, 20);
        addHSlider(kAddrLAPF2,  "APF L-Addr 2",   205, 460, 130, 40);   addTInput(kAddrLAPF2,   340, 480, 45, 20);
        addHSlider(kAddrRAPF2,  "APF R-Addr 2",   205, 500, 130, 40);   addTInput(kAddrRAPF2,   340, 520, 45, 20);

        addHSlider(kAddrLComb1, "Comb L-Addr 1",  400, 220, 130, 40);   addTInput(kAddrLComb1,  535, 240, 45, 20);
        addHSlider(kAddrRComb1, "Comb R-Addr 1",  400, 260, 130, 40);   addTInput(kAddrRComb1,  535, 280, 45, 20);
        addHSlider(kAddrLComb2, "Comb L-Addr 2",  400, 300, 130, 40);   addTInput(kAddrLComb2,  535, 320, 45, 20);
        addHSlider(kAddrRComb2, "Comb R-Addr 2",  400, 340, 130, 40);   addTInput(kAddrRComb2,  535, 360, 45, 20);
        addHSlider(kAddrLComb3, "Comb L-Addr 3",  400, 380, 130, 40);   addTInput(kAddrLComb3,  535, 400, 45, 20);
        addHSlider(kAddrRComb3, "Comb R-Addr 3",  400, 420, 130, 40);   addTInput(kAddrRComb3,  535, 440, 45, 20);
        addHSlider(kAddrLComb4, "Comb L-Addr 4",  400, 460, 130, 40);   addTInput(kAddrLComb4,  535, 480, 45, 20);
        addHSlider(kAddrRComb4, "Comb R-Addr 4",  400, 500, 130, 40);   addTInput(kAddrRComb4,  535, 520, 45, 20);

        addHSlider(kAddrLSame1, "SSR L-Addr 1",   595, 220, 130, 40);   addTInput(kAddrLSame1,  730, 240, 45, 20);
        addHSlider(kAddrRSame1, "SSR R-Addr 1",   595, 260, 130, 40);   addTInput(kAddrRSame1,  730, 280, 45, 20);
        addHSlider(kAddrLSame2, "SSR L-Addr 2",   595, 300, 130, 40);   addTInput(kAddrLSame2,  730, 320, 45, 20);
        addHSlider(kAddrRSame2, "SSR R-Addr 2",   595, 340, 130, 40);   addTInput(kAddrRSame2,  730, 360, 45, 20);
        addHSlider(kAddrLDiff1, "DSR L-Addr 1",   595, 380, 130, 40);   addTInput(kAddrLDiff1,  730, 400, 45, 20);
        addHSlider(kAddrRDiff1, "DSR R-Addr 1",   595, 420, 130, 40);   addTInput(kAddrRDiff1,  730, 440, 45, 20);
        addHSlider(kAddrLDiff2, "DSR L-Addr 2",   595, 460, 130, 40);   addTInput(kAddrLDiff2,  730, 480, 45, 20);
        addHSlider(kAddrRDiff2, "DSR R-Addr 2",   595, 500, 130, 40);   addTInput(kAddrRDiff2,  730, 520, 45, 20);
    };
}

#endif  // #if IPLUG_EDITOR

#if IPLUG_DSP

//------------------------------------------------------------------------------------------------------------------------------------------
// Called by the SPU emulation to retrieve an input sample.
// Returns a sample that was passed as input to the reverb effect.
//------------------------------------------------------------------------------------------------------------------------------------------
Spu::StereoSample PsxReverb::SpuWantsASampleCallback(void* pUserData) noexcept {
    PsxReverb& reverbPlugin = *(PsxReverb*) pUserData;
    return reverbPlugin.mSpuInputSample;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Setup DSP related stuff
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::DoDspSetup() noexcept {
    // Create the PlayStation SPU core and do it with NO voices, since we are not playing any samples and just using the reverb FX...
    Spu::initCore(mSpu, kSpuRamSize, 0);

    // Set default volume levels
    mSpu.masterVol.left = 0x3FFF;
    mSpu.masterVol.right = 0x3FFF;
    mSpu.reverbVol.left = 0x2FFF;
    mSpu.reverbVol.right = 0x2FFF;
    mSpu.extInputVol.left = 0x7FFF;
    mSpu.extInputVol.right = 0x7FFF;

    // Setup other SPU settings
    mSpu.bUnmute = true;
    mSpu.bReverbWriteEnable = true;
    mSpu.bExtEnabled = true;
    mSpu.bExtReverbEnable = true;
    mSpu.cycleCount = 0;
    mSpu.reverbBaseAddr8 = 0;
    mSpu.reverbCurAddr = 0;
    mSpu.processedReverb = {};
    mSpu.reverbRegs = {};

    // This is how we will feed samples into the SPU which were fed to the this plugin
    mSpu.pExtInputCallback = SpuWantsASampleCallback;
    mSpu.pExtInputUserData = this;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Called when a parameter changes
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::InformHostOfParamChange([[maybe_unused]] int idx, [[maybe_unused]] double normalizedValue) noexcept {
    UpdateSpuRegistersFromParams();

    // If changing the work area base address then clear it
    if (idx == kWABaseAddr) {
        ClearReverbWorkArea();
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Called when a preset changes
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::OnRestoreState() noexcept {
    Plugin::OnRestoreState();
    UpdateSpuRegistersFromParams();
    ClearReverbWorkArea();  // When switching patches stop the current reverb effect
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Upates the value of the PlayStation SPUs reverb registers which are bound to certain parameters
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::UpdateSpuRegistersFromParams() noexcept {
    std::lock_guard<std::recursive_mutex> lockSpu(mSpuMutex);

    mSpu.masterVol.left           = (int16_t) GetParam(kMasterVolL)->Value();
    mSpu.masterVol.right          = (int16_t) GetParam(kMasterVolR)->Value();
    mSpu.extInputVol.left         = (int16_t) GetParam(kInputVolL)->Value();
    mSpu.extInputVol.right        = (int16_t) GetParam(kInputVolR)->Value();
    mSpu.reverbVol.left           = (int16_t) GetParam(kReverbVolL)->Value();
    mSpu.reverbVol.right          = (int16_t) GetParam(kReverbVolR)->Value();
    mSpu.reverbBaseAddr8          = (uint16_t) GetParam(kWABaseAddr)->Value();
    mSpu.reverbRegs.dispAPF1      = (uint16_t) GetParam(kDispAPF1)->Value();
    mSpu.reverbRegs.dispAPF2      = (uint16_t) GetParam(kDispAPF2)->Value();
    mSpu.reverbRegs.volIIR        = (int16_t) GetParam(kVolIIR)->Value();
    mSpu.reverbRegs.volComb1      = (int16_t) GetParam(kVolComb1)->Value();
    mSpu.reverbRegs.volComb2      = (int16_t) GetParam(kVolComb2)->Value();
    mSpu.reverbRegs.volComb3      = (int16_t) GetParam(kVolComb3)->Value();
    mSpu.reverbRegs.volComb4      = (int16_t) GetParam(kVolComb4)->Value();
    mSpu.reverbRegs.volWall       = (int16_t) GetParam(kVolWall)->Value();
    mSpu.reverbRegs.volAPF1       = (int16_t) GetParam(kVolAPF1)->Value();
    mSpu.reverbRegs.volAPF2       = (int16_t) GetParam(kVolAPF2)->Value();
    mSpu.reverbRegs.addrLSame1    = (uint16_t) GetParam(kAddrLSame1)->Value();
    mSpu.reverbRegs.addrRSame1    = (uint16_t) GetParam(kAddrRSame1)->Value();
    mSpu.reverbRegs.addrLComb1    = (uint16_t) GetParam(kAddrLComb1)->Value();
    mSpu.reverbRegs.addrRComb1    = (uint16_t) GetParam(kAddrRComb1)->Value();
    mSpu.reverbRegs.addrLComb2    = (uint16_t) GetParam(kAddrLComb2)->Value();
    mSpu.reverbRegs.addrRComb2    = (uint16_t) GetParam(kAddrRComb2)->Value();
    mSpu.reverbRegs.addrLSame2    = (uint16_t) GetParam(kAddrLSame2)->Value();
    mSpu.reverbRegs.addrRSame2    = (uint16_t) GetParam(kAddrRSame2)->Value();
    mSpu.reverbRegs.addrLDiff1    = (uint16_t) GetParam(kAddrLDiff1)->Value();
    mSpu.reverbRegs.addrRDiff1    = (uint16_t) GetParam(kAddrRDiff1)->Value();
    mSpu.reverbRegs.addrLComb3    = (uint16_t) GetParam(kAddrLComb3)->Value();
    mSpu.reverbRegs.addrRComb3    = (uint16_t) GetParam(kAddrRComb3)->Value();
    mSpu.reverbRegs.addrLComb4    = (uint16_t) GetParam(kAddrLComb4)->Value();
    mSpu.reverbRegs.addrRComb4    = (uint16_t) GetParam(kAddrRComb4)->Value();
    mSpu.reverbRegs.addrLDiff2    = (uint16_t) GetParam(kAddrLDiff2)->Value();
    mSpu.reverbRegs.addrRDiff2    = (uint16_t) GetParam(kAddrRDiff2)->Value();
    mSpu.reverbRegs.addrLAPF1     = (uint16_t) GetParam(kAddrLAPF1)->Value();
    mSpu.reverbRegs.addrRAPF1     = (uint16_t) GetParam(kAddrRAPF1)->Value();
    mSpu.reverbRegs.addrLAPF2     = (uint16_t) GetParam(kAddrLAPF2)->Value();
    mSpu.reverbRegs.addrRAPF2     = (uint16_t) GetParam(kAddrRAPF2)->Value();
    mSpu.reverbRegs.volLIn        = (int16_t) GetParam(kVolLIn)->Value();
    mSpu.reverbRegs.volRIn        = (int16_t) GetParam(kVolRIn)->Value();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Clears the work area for the current reverb effect, effectively silencing the current reverb
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::ClearReverbWorkArea() noexcept {
    // Just clear the entire SPU ram...
    std::memset(mSpu.pRam, 0, 512 * 1024);
}

#endif  // #if IPLUG_DSP
