#pragma once

#include "IPlug_include_in_plug_hdr.h"

#include "../../PluginsCommon/Spu.h"
#include <mutex>

using namespace iplug;
using namespace igraphics;

//------------------------------------------------------------------------------------------------------------------------------------------
// These parameters are the reverb registers used by the PlayStation 1.
// The registers define the reverb settings and how the reverb is processed.
//------------------------------------------------------------------------------------------------------------------------------------------
enum EParams : uint32_t {
    kMasterVolL,    // Spu master volume multiplier - left
    kMasterVolR,    // Spu master volume multiplier - right
    kInputVolL,     // Spu input volume multiplier - left
    kInputVolR,     // Spu input volume multiplier - right
    kReverbVolL,    // Reverb volume/depth multiplier - left
    kReverbVolR,    // Reverb volume/depth multiplier - right
    kWABaseAddr,    // Reverb work area base address (in multiples of '8') - any point past this in SPU RAM is used for the reverb effect
    kDispAPF1,      // Reverb APF Offset 1
    kDispAPF2,      // Reverb APF Offset 2
    kVolIIR,        // Reverb Reflection Volume 1
    kVolComb1,      // Reverb Comb Volume 1
    kVolComb2,      // Reverb Comb Volume 2
    kVolComb3,      // Reverb Comb Volume 3
    kVolComb4,      // Reverb Comb Volume 4
    kVolWall,       // Reverb Reflection Volume 2
    kVolAPF1,       // Reverb APF Volume 1
    kVolAPF2,       // Reverb APF Volume 2
    kAddrLSame1,    // Reverb Same Side Reflection Address 1: Left
    kAddrRSame1,    // Reverb Same Side Reflection Address 1: Right
    kAddrLComb1,    // Reverb Comb Address 1: Left
    kAddrRComb1,    // Reverb Comb Address 1: Right
    kAddrLComb2,    // Reverb Comb Address 2: Left
    kAddrRComb2,    // Reverb Comb Address 2: Right
    kAddrLSame2,    // Reverb Same Side Reflection Address 2: Left
    kAddrRSame2,    // Reverb Same Side Reflection Address 2: Right
    kAddrLDiff1,    // Reverb Different Side Reflect Address 1: Left
    kAddrRDiff1,    // Reverb Different Side Reflect Address 1: Right
    kAddrLComb3,    // Reverb Comb Address 3: Left
    kAddrRComb3,    // Reverb Comb Address 3: Right
    kAddrLComb4,    // Reverb Comb Address 4: Left
    kAddrRComb4,    // Reverb Comb Address 4: Right
    kAddrLDiff2,    // Reverb Different Side Reflect Address 2: Left
    kAddrRDiff2,    // Reverb Different Side Reflect Address 2: Right
    kAddrLAPF1,     // Reverb APF Address 1: Left
    kAddrRAPF1,     // Reverb APF Address 1: Right
    kAddrLAPF2,     // Reverb APF Address 2: Left
    kAddrRAPF2,     // Reverb APF Address 2: Right
    kVolLIn,        // Reverb Input Volume: Left
    kVolRIn,        // Reverb Input Volume: Right
    kNumParams
};

//------------------------------------------------------------------------------------------------------------------------------------------
// Logic for the PlayStation 1 reverb plugin
//------------------------------------------------------------------------------------------------------------------------------------------
class PsxReverb final : public Plugin {
public:
    PsxReverb(const InstanceInfo& info) noexcept;
    virtual ~PsxReverb() noexcept override;

    #if IPLUG_DSP
        void ProcessBlock(sample** pInputs, sample** pOutputs, int numFrames) noexcept override;
    #endif

private:
    #if IPLUG_DSP
        Spu::Core               mSpu;
        std::recursive_mutex    mSpuMutex;
        Spu::StereoSample       mSpuInputSample;
    #endif

    void DefinePluginParams() noexcept;
    void DefinePluginPresets() noexcept;

    #if IPLUG_EDITOR
        void DoEditorSetup() noexcept;
    #endif

    #if IPLUG_DSP
        static Spu::StereoSample SpuWantsASampleCallback(void* pUserData) noexcept;
        void DoDspSetup() noexcept;
        virtual void InformHostOfParamChange(int idx, double normalizedValue) noexcept override;
        virtual void OnRestoreState() noexcept override;
        void UpdateSpuRegistersFromParams() noexcept;
        void ClearReverbWorkArea() noexcept;
    #endif
};
