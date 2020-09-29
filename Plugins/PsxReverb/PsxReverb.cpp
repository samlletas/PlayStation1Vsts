#include "PsxReverb.h"

#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

// How many reverb presets there are
static constexpr int kNumPresets = 10;

//------------------------------------------------------------------------------------------------------------------------------------------
// Initializes the reverb plugin
//------------------------------------------------------------------------------------------------------------------------------------------
PsxReverb::PsxReverb(const InstanceInfo& info)
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
void PsxReverb::ProcessBlock(sample** pInputs, sample** pOutputs, int numFrames) {
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
    GetParam(kVolLIn)->InitInt("volLIn", 0, INT16_MIN, INT16_MAX, "Volume");
    GetParam(kVolRIn)->InitInt("volRIn", 0, INT16_MIN, INT16_MAX, "Volume");
    GetParam(kVolIIR)->InitInt("volIIR", 0, INT16_MIN, INT16_MAX, "Volume");
    GetParam(kVolWall)->InitInt("volWall", 0, INT16_MIN, INT16_MAX, "Volume");
    GetParam(kVolAPF1)->InitInt("volAPF1", 0, INT16_MIN, INT16_MAX, "Volume");
    GetParam(kVolAPF2)->InitInt("volAPF2", 0, INT16_MIN, INT16_MAX, "Volume");
    GetParam(kVolComb1)->InitInt("volComb1", 0, INT16_MIN, INT16_MAX, "Volume");
    GetParam(kVolComb2)->InitInt("volComb2", 0, INT16_MIN, INT16_MAX, "Volume");

    GetParam(kVolComb3)->InitInt("volComb3", 0, INT16_MIN, INT16_MAX, "Volume");
    GetParam(kVolComb4)->InitInt("volComb4", 0, INT16_MIN, INT16_MAX, "Volume");
    GetParam(kDispAPF1)->InitInt("dispAPF1", 0, 0, UINT16_MAX, "Offset");
    GetParam(kDispAPF2)->InitInt("dispAPF2", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrLAPF1)->InitInt("addrLAPF1", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrRAPF1)->InitInt("addrRAPF1", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrLAPF2)->InitInt("addrLAPF2", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrRAPF2)->InitInt("addrRAPF2", 0, 0, UINT16_MAX, "Offset");

    GetParam(kAddrLComb1)->InitInt("addrLComb1", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrRComb1)->InitInt("addrRComb1", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrLComb2)->InitInt("addrLComb2", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrRComb2)->InitInt("addrRComb2", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrLComb3)->InitInt("addrLComb3", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrRComb3)->InitInt("addrRComb3", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrLComb4)->InitInt("addrLComb4", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrRComb4)->InitInt("addrRComb4", 0, 0, UINT16_MAX, "Offset");

    GetParam(kAddrLSame1)->InitInt("addrLSame1", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrRSame1)->InitInt("addrRSame1", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrLSame2)->InitInt("addrLSame2", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrRSame2)->InitInt("addrRSame2", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrLDiff1)->InitInt("addrLDiff1", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrRDiff1)->InitInt("addrRDiff1", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrLDiff2)->InitInt("addrLDiff2", 0, 0, UINT16_MAX, "Offset");
    GetParam(kAddrRDiff2)->InitInt("addrRDiff2", 0, 0, UINT16_MAX, "Offset");
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Defines the presets for the effect plugin.
// These are the actual effect presets found in the PsyQ SDK.
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::DefinePluginPresets() noexcept {
    MakePreset(
        "Off",
        0x0,        // apfOffset1
        0x0,        // apfOffset2
        0x0,        // reflectionVolume1
        0x0,        // combVolume1
        0x0,        // combVolume2
        0x0,        // combVolume3
        0x0,        // combVolume4
        0x0,        // reflectionVolume2
        0x0,        // apfVolume1
        0x0,        // apfVolume2
        0x0,        // sameSideRefractAddr1Left
        0x0,        // sameSideRefractAddr1Right
        0x0,        // combAddr1Left
        0x0,        // combAddr1Right
        0x0,        // combAddr2Left
        0x0,        // combAddr2Right
        0x0,        // sameSideRefractAddr2Left
        0x0,        // sameSideRefractAddr2Right
        0x0,        // diffSideReflectAddr1Left
        0x0,        // diffSideReflectAddr1Right
        0x0,        // combAddr3Left
        0x0,        // combAddr3Right
        0x0,        // combAddr4Left
        0x0,        // combAddr4Right
        0x0,        // diffSideReflectAddr2Left
        0x0,        // diffSideReflectAddr2Right
        0x0,        // apfAddr1Left
        0x0,        // apfAddr1Right
        0x0,        // apfAddr2Left
        0x0,        // apfAddr2Right
        0x0,        // inputVolLeft
        0x0         // inputVolRight
    );

    MakePreset(
        "Room",
        0x7D,       // apfOffset1
        0x5B,       // apfOffset2
        0x6D80,     // reflectionVolume1
        0x54B8,     // combVolume1
        0xBED0,     // combVolume2
        0x0,        // combVolume3
        0x0,        // combVolume4
        0xBA80,     // reflectionVolume2
        0x5800,     // apfVolume1
        0x5300,     // apfVolume2
        0x4D6,      // sameSideRefractAddr1Left
        0x333,      // sameSideRefractAddr1Right
        0x3F0,      // combAddr1Left
        0x227,      // combAddr1Right
        0x374,      // combAddr2Left
        0x1EF,      // combAddr2Right
        0x334,      // sameSideRefractAddr2Left
        0x1B5,      // sameSideRefractAddr2Right
        0x0,        // diffSideReflectAddr1Left
        0x0,        // diffSideReflectAddr1Right
        0x0,        // combAddr3Left
        0x0,        // combAddr3Right
        0x0,        // combAddr4Left
        0x0,        // combAddr4Right
        0x0,        // diffSideReflectAddr2Left
        0x0,        // diffSideReflectAddr2Right
        0x1B4,      // apfAddr1Left
        0x136,      // apfAddr1Right
        0xB8,       // apfAddr2Left
        0x5C,       // apfAddr2Right
        0x8000,     // inputVolLeft
        0x8000      // inputVolRight
    );

    MakePreset(
        "Studio A",
        0x33,       // apfOffset1
        0x25,       // apfOffset2
        0x70F0,     // reflectionVolume1
        0x4FA8,     // combVolume1
        0xBCE0,     // combVolume2
        0x4410,     // combVolume3
        0xC0F0,     // combVolume4
        0x9C00,     // reflectionVolume2
        0x5280,     // apfVolume1
        0x4EC0,     // apfVolume2
        0x3E4,      // sameSideRefractAddr1Left
        0x31B,      // sameSideRefractAddr1Right
        0x3A4,      // combAddr1Left
        0x2AF,      // combAddr1Right
        0x372,      // combAddr2Left
        0x266,      // combAddr2Right
        0x31C,      // sameSideRefractAddr2Left
        0x25D,      // sameSideRefractAddr2Right
        0x25C,      // diffSideReflectAddr1Left
        0x18E,      // diffSideReflectAddr1Right
        0x22F,      // combAddr3Left
        0x135,      // combAddr3Right
        0x1D2,      // combAddr4Left
        0xB7,       // combAddr4Right
        0x18F,      // diffSideReflectAddr2Left
        0xB5,       // diffSideReflectAddr2Right
        0xB4,       // apfAddr1Left
        0x80,       // apfAddr1Right
        0x4C,       // apfAddr2Left
        0x26,       // apfAddr2Right
        0x8000,     // inputVolLeft
        0x8000      // inputVolRight
    );

    MakePreset(
        "Studio B",
        0xB1,       // apfOffset1
        0x7F,       // apfOffset2
        0x70F0,     // reflectionVolume1
        0x4FA8,     // combVolume1
        0xBCE0,     // combVolume2
        0x4510,     // combVolume3
        0xBEF0,     // combVolume4
        0xB4C0,     // reflectionVolume2
        0x5280,     // apfVolume1
        0x4EC0,     // apfVolume2
        0x904,      // sameSideRefractAddr1Left
        0x76B,      // sameSideRefractAddr1Right
        0x824,      // combAddr1Left
        0x65F,      // combAddr1Right
        0x7A2,      // combAddr2Left
        0x616,      // combAddr2Right
        0x76C,      // sameSideRefractAddr2Left
        0x5ED,      // sameSideRefractAddr2Right
        0x5EC,      // diffSideReflectAddr1Left
        0x42E,      // diffSideReflectAddr1Right
        0x50F,      // combAddr3Left
        0x305,      // combAddr3Right
        0x462,      // combAddr4Left
        0x2B7,      // combAddr4Right
        0x42F,      // diffSideReflectAddr2Left
        0x265,      // diffSideReflectAddr2Right
        0x264,      // apfAddr1Left
        0x1B2,      // apfAddr1Right
        0x100,      // apfAddr2Left
        0x80,       // apfAddr2Right
        0x8000,     // inputVolLeft
        0x8000      // inputVolRight
    );

    MakePreset(
        "Studio C",
        0xE3,       // apfOffset1
        0xA9,       // apfOffset2
        0x6F60,     // reflectionVolume1
        0x4FA8,     // combVolume1
        0xBCE0,     // combVolume2
        0x4510,     // combVolume3
        0xBEF0,     // combVolume4
        0xA680,     // reflectionVolume2
        0x5680,     // apfVolume1
        0x52C0,     // apfVolume2
        0xDFB,      // sameSideRefractAddr1Left
        0xB58,      // sameSideRefractAddr1Right
        0xD09,      // combAddr1Left
        0xA3C,      // combAddr1Right
        0xBD9,      // combAddr2Left
        0x973,      // combAddr2Right
        0xB59,      // sameSideRefractAddr2Left
        0x8DA,      // sameSideRefractAddr2Right
        0x8D9,      // diffSideReflectAddr1Left
        0x5E9,      // diffSideReflectAddr1Right
        0x7EC,      // combAddr3Left
        0x4B0,      // combAddr3Right
        0x6EF,      // combAddr4Left
        0x3D2,      // combAddr4Right
        0x5EA,      // diffSideReflectAddr2Left
        0x31D,      // diffSideReflectAddr2Right
        0x31C,      // apfAddr1Left
        0x238,      // apfAddr1Right
        0x154,      // apfAddr2Left
        0xAA,       // apfAddr2Right
        0x8000,     // inputVolLeft
        0x8000      // inputVolRight
    );

    MakePreset(
        "Hall",
        0x1A5,      // apfOffset1
        0x139,      // apfOffset2
        0x6000,     // reflectionVolume1
        0x5000,     // combVolume1
        0x4C00,     // combVolume2
        0xB800,     // combVolume3
        0xBC00,     // combVolume4
        0xC000,     // reflectionVolume2
        0x6000,     // apfVolume1
        0x5C00,     // apfVolume2
        0x15BA,     // sameSideRefractAddr1Left
        0x11BB,     // sameSideRefractAddr1Right
        0x14C2,     // combAddr1Left
        0x10BD,     // combAddr1Right
        0x11BC,     // combAddr2Left
        0xDC1,      // combAddr2Right
        0x11C0,     // sameSideRefractAddr2Left
        0xDC3,      // sameSideRefractAddr2Right
        0xDC0,      // diffSideReflectAddr1Left
        0x9C1,      // diffSideReflectAddr1Right
        0xBC4,      // combAddr3Left
        0x7C1,      // combAddr3Right
        0xA00,      // combAddr4Left
        0x6CD,      // combAddr4Right
        0x9C2,      // diffSideReflectAddr2Left
        0x5C1,      // diffSideReflectAddr2Right
        0x5C0,      // apfAddr1Left
        0x41A,      // apfAddr1Right
        0x274,      // apfAddr2Left
        0x13A,      // apfAddr2Right
        0x8000,     // inputVolLeft
        0x8000      // inputVolRight
    );

    MakePreset(
        "Space",
        0x33D,      // apfOffset1
        0x231,      // apfOffset2
        0x7E00,     // reflectionVolume1
        0x5000,     // combVolume1
        0xB400,     // combVolume2
        0xB000,     // combVolume3
        0x4C00,     // combVolume4
        0xB000,     // reflectionVolume2
        0x6000,     // apfVolume1
        0x5400,     // apfVolume2
        0x1ED6,     // sameSideRefractAddr1Left
        0x1A31,     // sameSideRefractAddr1Right
        0x1D14,     // combAddr1Left
        0x183B,     // combAddr1Right
        0x1BC2,     // combAddr2Left
        0x16B2,     // combAddr2Right
        0x1A32,     // sameSideRefractAddr2Left
        0x15EF,     // sameSideRefractAddr2Right
        0x15EE,     // diffSideReflectAddr1Left
        0x1055,     // diffSideReflectAddr1Right
        0x1334,     // combAddr3Left
        0xF2D,      // combAddr3Right
        0x11F6,     // combAddr4Left
        0xC5D,      // combAddr4Right
        0x1056,     // diffSideReflectAddr2Left
        0xAE1,      // diffSideReflectAddr2Right
        0xAE0,      // apfAddr1Left
        0x7A2,      // apfAddr1Right
        0x464,      // apfAddr2Left
        0x232,      // apfAddr2Right
        0x8000,     // inputVolLeft
        0x8000      // inputVolRight
    );

    MakePreset(
        "Echo",
        0x1,        // apfOffset1
        0x1,        // apfOffset2
        0x7FFF,     // reflectionVolume1
        0x7FFF,     // combVolume1
        0x0,        // combVolume2
        0x0,        // combVolume3
        0x0,        // combVolume4
        0x8100,     // reflectionVolume2
        0x0,        // apfVolume1
        0x0,        // apfVolume2
        0x1FFF,     // sameSideRefractAddr1Left
        0xFFF,      // sameSideRefractAddr1Right
        0x1005,     // combAddr1Left
        0x5,        // combAddr1Right
        0x0,        // combAddr2Left
        0x0,        // combAddr2Right
        0x1005,     // sameSideRefractAddr2Left
        0x5,        // sameSideRefractAddr2Right
        0x0,        // diffSideReflectAddr1Left
        0x0,        // diffSideReflectAddr1Right
        0x0,        // combAddr3Left
        0x0,        // combAddr3Right
        0x0,        // combAddr4Left
        0x0,        // combAddr4Right
        0x0,        // diffSideReflectAddr2Left
        0x0,        // diffSideReflectAddr2Right
        0x1004,     // apfAddr1Left
        0x1002,     // apfAddr1Right
        0x4,        // apfAddr2Left
        0x2,        // apfAddr2Right
        0x8000,     // inputVolLeft
        0x8000      // inputVolRight
    );

    MakePreset(
        "Delay",
        0x1,        // apfOffset1
        0x1,        // apfOffset2
        0x7FFF,     // reflectionVolume1
        0x7FFF,     // combVolume1
        0x0,        // combVolume2
        0x0,        // combVolume3
        0x0,        // combVolume4
        0x0,        // reflectionVolume2
        0x0,        // apfVolume1
        0x0,        // apfVolume2
        0x1FFF,     // sameSideRefractAddr1Left
        0xFFF,      // sameSideRefractAddr1Right
        0x1005,     // combAddr1Left
        0x5,        // combAddr1Right
        0x0,        // combAddr2Left
        0x0,        // combAddr2Right
        0x1005,     // sameSideRefractAddr2Left
        0x5,        // sameSideRefractAddr2Right
        0x0,        // diffSideReflectAddr1Left
        0x0,        // diffSideReflectAddr1Right
        0x0,        // combAddr3Left
        0x0,        // combAddr3Right
        0x0,        // combAddr4Left
        0x0,        // combAddr4Right
        0x0,        // diffSideReflectAddr2Left
        0x0,        // diffSideReflectAddr2Right
        0x1004,     // apfAddr1Left
        0x1002,     // apfAddr1Right
        0x4,        // apfAddr2Left
        0x2,        // apfAddr2Right
        0x8000,     // inputVolLeft
        0x8000      // inputVolRight
    );

    MakePreset(
        "Pipe",
        0x0,        // fieldBits
        0x17,       // apfOffset1
        0x13,       // apfOffset2
        0x70F0,     // reflectionVolume1
        0x4FA8,     // combVolume1
        0xBCE0,     // combVolume2
        0x4510,     // combVolume3
        0xBEF0,     // combVolume4
        0x8500,     // reflectionVolume2
        0x5F80,     // apfVolume1
        0x54C0,     // apfVolume2
        0x371,      // sameSideRefractAddr1Left
        0x2AF,      // sameSideRefractAddr1Right
        0x2E5,      // combAddr1Left
        0x1DF,      // combAddr1Right
        0x2B0,      // combAddr2Left
        0x1D7,      // combAddr2Right
        0x358,      // sameSideRefractAddr2Left
        0x26A,      // sameSideRefractAddr2Right
        0x1D6,      // diffSideReflectAddr1Left
        0x11E,      // diffSideReflectAddr1Right
        0x12D,      // combAddr3Left
        0xB1,       // combAddr3Right
        0x11F,      // combAddr4Left
        0x59,       // combAddr4Right
        0x1A0,      // diffSideReflectAddr2Left
        0xE3,       // diffSideReflectAddr2Right
        0x58,       // apfAddr1Left
        0x40,       // apfAddr1Right
        0x28,       // apfAddr2Left
        0x14,       // apfAddr2Right
        0x8000,     // inputVolLeft
        0x8000      // inputVolRight
    );
}

#if IPLUG_EDITOR

//------------------------------------------------------------------------------------------------------------------------------------------
// Setup controls for the plugin's GUI
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::DoEditorSetup() {
    mMakeGraphicsFunc = [&]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_HEIGHT));
    };
    
    mLayoutFunc = [&](IGraphics* pGraphics) {
        pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
        pGraphics->AttachPanelBackground(COLOR_GRAY);
        pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

        pGraphics->AttachControl(new IVBakedPresetManagerControl(IRECT(0.0f, 0.0f, 600.0f, 40.0f), DEFAULT_STYLE));

        pGraphics->AttachControl(new IVSliderControl(IRECT( 10.0f,  50.0f, 190.0f,  90.0f), kVolLIn,      "In L-Vol",       DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT( 10.0f, 100.0f, 190.0f, 140.0f), kVolRIn,      "In R-Vol",       DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT( 10.0f, 150.0f, 190.0f, 190.0f), kVolIIR,      "Refl Vol 1",     DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT( 10.0f, 200.0f, 190.0f, 240.0f), kVolWall,     "Refl Vol 2",     DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT( 10.0f, 250.0f, 190.0f, 290.0f), kVolAPF1,     "APF Vol 1",      DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT( 10.0f, 300.0f, 190.0f, 340.0f), kVolAPF2,     "APF Vol 2",      DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT( 10.0f, 350.0f, 190.0f, 390.0f), kVolComb1,    "Comb Vol 1",     DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT( 10.0f, 400.0f, 190.0f, 440.0f), kVolComb2,    "Comb Vol 2",     DEFAULT_STYLE, true, EDirection::Horizontal));

        pGraphics->AttachControl(new IVSliderControl(IRECT(200.0f,  50.0f, 390.0f,  90.0f), kVolComb3,    "Comb Vol 3",     DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(200.0f, 100.0f, 390.0f, 140.0f), kVolComb4,    "Comb Vol 4",     DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(200.0f, 150.0f, 390.0f, 190.0f), kDispAPF1,    "APF Offset 1",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(200.0f, 200.0f, 390.0f, 240.0f), kDispAPF2,    "APF Offset 2",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(200.0f, 250.0f, 390.0f, 290.0f), kAddrLAPF1,   "APF L-Addr 1",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(200.0f, 300.0f, 390.0f, 340.0f), kAddrRAPF1,   "APF R-Addr 1",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(200.0f, 350.0f, 390.0f, 390.0f), kAddrLAPF2,   "APF L-Addr 2",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(200.0f, 400.0f, 390.0f, 440.0f), kAddrRAPF2,   "APF R-Addr 2",   DEFAULT_STYLE, true, EDirection::Horizontal));

        pGraphics->AttachControl(new IVSliderControl(IRECT(400.0f,  50.0f, 590.0f,  90.0f), kAddrLComb1,  "Comb L-Addr 1",  DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(400.0f, 100.0f, 590.0f, 140.0f), kAddrRComb1,  "Comb R-Addr 1",  DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(400.0f, 150.0f, 590.0f, 190.0f), kAddrLComb2,  "Comb L-Addr 2",  DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(400.0f, 200.0f, 590.0f, 240.0f), kAddrRComb2,  "Comb R-Addr 2",  DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(400.0f, 250.0f, 590.0f, 290.0f), kAddrLComb3,  "Comb L-Addr 3",  DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(400.0f, 300.0f, 590.0f, 340.0f), kAddrRComb3,  "Comb R-Addr 3",  DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(400.0f, 350.0f, 590.0f, 390.0f), kAddrLComb4,  "Comb L-Addr 4",  DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(400.0f, 400.0f, 590.0f, 440.0f), kAddrRComb4,  "Comb R-Addr 4",  DEFAULT_STYLE, true, EDirection::Horizontal));

        pGraphics->AttachControl(new IVSliderControl(IRECT(600.0f,  50.0f, 790.0f,  90.0f), kAddrLSame1,  "SSR L-Addr 1",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(600.0f, 100.0f, 790.0f, 140.0f), kAddrRSame1,  "SSR R-Addr 1",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(600.0f, 150.0f, 790.0f, 190.0f), kAddrLSame2,  "SSR L-Addr 2",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(600.0f, 200.0f, 790.0f, 240.0f), kAddrRSame2,  "SSR R-Addr 2",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(600.0f, 250.0f, 790.0f, 290.0f), kAddrLDiff1,  "DSR L-Addr 1",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(600.0f, 300.0f, 790.0f, 340.0f), kAddrRDiff1,  "DSR R-Addr 1",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(600.0f, 350.0f, 790.0f, 390.0f), kAddrLDiff2,  "DSR L-Addr 2",   DEFAULT_STYLE, true, EDirection::Horizontal));
        pGraphics->AttachControl(new IVSliderControl(IRECT(600.0f, 400.0f, 790.0f, 440.0f), kAddrRDiff2,  "DSR R-Addr 2",   DEFAULT_STYLE, true, EDirection::Horizontal));
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
void PsxReverb::DoDspSetup() {
    // Create the PlayStation SPU core with the standard 512 KiB RAM but NO voices (since we are not playing any samples)
    Spu::initCore(mSpu, 1024 * 512, 0);

    // Set volume levels
    mSpu.masterVol.left = 0x3FFF;
    mSpu.masterVol.right = 0x3FFF;
    mSpu.reverbVol.left = 0x3FFF;
    mSpu.reverbVol.right = 0x3FFF;
    mSpu.extInputVol.left = 0x3FFF;
    mSpu.extInputVol.right = 0x3FFF;

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
void PsxReverb::InformHostOfParamChange([[maybe_unused]] int idx, [[maybe_unused]] double normalizedValue) {
    UpdateSpuReverbRegisters();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Called when a preset changes
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::OnRestoreState() {
    Plugin::OnRestoreState();
    UpdateSpuReverbRegisters();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Upates the value of the PlayStation SPUs reverb registers
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxReverb::UpdateSpuReverbRegisters() {
    std::lock_guard<std::recursive_mutex> lockSpu(mSpuMutex);

    // Update the registers
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

    // Determine the reverb base address: doing this the way the PsyQ SDK did it.
    //  (1) Figure out the max displacement or offset of all reverb address variables (in multiples of 8 bytes).
    //  (2) Subtract from the end SPU memory address (in multiples of 8 bytes) of 0x10000
    //  (3) Subtract a further '2' units (16 bytes) to get the base addresss.
    //
    uint32_t maxAddrDisp8 = std::max<uint32_t>(mSpu.reverbRegs.dispAPF1, mSpu.reverbRegs.dispAPF2);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrLSame1);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrRSame1);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrLComb1);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrRComb1);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrLComb2);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrRComb2);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrLSame2);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrRSame2);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrLDiff1);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrRDiff1);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrLComb3);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrRComb3);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrLComb4);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrRComb4);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrLDiff2);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrRDiff2);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrLAPF1);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrRAPF1);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrLAPF2);
    maxAddrDisp8 = std::max<uint32_t>(maxAddrDisp8, mSpu.reverbRegs.addrRAPF2);
    maxAddrDisp8 += 2;
    maxAddrDisp8 = std::min<uint32_t>(maxAddrDisp8, 0x10000u);

    // Set the reverb base address
    mSpu.reverbBaseAddr8 = 0x10000u - maxAddrDisp8;

    // Clear the reverb working area while we are at this also: this will stop any reverb effect in flight
    std::memset(mSpu.pRam, 0, 512 * 1024);
}

#endif  // #if IPLUG_DSP
