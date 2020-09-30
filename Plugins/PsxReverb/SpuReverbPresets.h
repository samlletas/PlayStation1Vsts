#pragma once

#include "../PluginsCommon/Macros.h"
#include <cstdint>

//------------------------------------------------------------------------------------------------------------------------------------------
// PlayStation PsyQ SDK reverb definitions borrowed from PsyDoom.
// These are used to create the presets for this reverb plugin.
//------------------------------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(SpuReverbPresets)

//------------------------------------------------------------------------------------------------------------------------------------------
// An internal LIBSPU struct used to hold a definition for a reverb type, before sending the settings to the SPU.
// All of these fields (except the 'paramBits' field) will map directly to SPU registers controlling reverb.
// For more details on what the reverb fields mean, see the NO$PSX specs:
//  https://problemkaputt.de/psx-spx.htm#spureverbregisters
//
// Notes:
//  (1) This is modified slightly from PsyDoom to remove the 'fieldBits' member which we don't need.
//  (2) All volume fields are actually 'int16_t' but are defined as 'uint16_t' for convenience with hex notation.
//------------------------------------------------------------------------------------------------------------------------------------------
struct SpuReverbDef {
    uint16_t    apfOffset1;
    uint16_t    apfOffset2;
    uint16_t    reflectionVolume1;
    uint16_t    combVolume1;
    uint16_t    combVolume2;
    uint16_t    combVolume3;
    uint16_t    combVolume4;
    uint16_t    reflectionVolume2;
    uint16_t    apfVolume1;
    uint16_t    apfVolume2;
    uint16_t    sameSideRefractAddr1Left;
    uint16_t    sameSideRefractAddr1Right;
    uint16_t    combAddr1Left;
    uint16_t    combAddr1Right;
    uint16_t    combAddr2Left;
    uint16_t    combAddr2Right;
    uint16_t    sameSideRefractAddr2Left;
    uint16_t    sameSideRefractAddr2Right;
    uint16_t    diffSideReflectAddr1Left;
    uint16_t    diffSideReflectAddr1Right;
    uint16_t    combAddr3Left;
    uint16_t    combAddr3Right;
    uint16_t    combAddr4Left;
    uint16_t    combAddr4Right;
    uint16_t    diffSideReflectAddr2Left;
    uint16_t    diffSideReflectAddr2Right;
    uint16_t    apfAddr1Left;
    uint16_t    apfAddr1Right;
    uint16_t    apfAddr2Left;
    uint16_t    apfAddr2Right;
    uint16_t    inputVolLeft;
    uint16_t    inputVolRight;
};

// Spu reverb modes
enum SpuReverbMode : int32_t {
    SPU_REV_MODE_OFF        = 0,
    SPU_REV_MODE_ROOM       = 1,
    SPU_REV_MODE_STUDIO_A   = 2,
    SPU_REV_MODE_STUDIO_B   = 3,
    SPU_REV_MODE_STUDIO_C   = 4,
    SPU_REV_MODE_HALL       = 5,
    SPU_REV_MODE_SPACE      = 6,
    SPU_REV_MODE_ECHO       = 7,
    SPU_REV_MODE_DELAY      = 8,
    SPU_REV_MODE_PIPE       = 9,
    SPU_REV_MODE_MAX        = 10
};

// Definitions for the 10 available reverb modes in the PsyQ SDK
extern const SpuReverbDef gReverbDefs[SPU_REV_MODE_MAX];

// The base address of the reverb working area for all 10 reverb modes, divided by '8'.
// Multiply by '8' to get the real address in SPU RAM where the reverb work area is located.
// Note that everything past that address in SPU RAM is reserved for reverb.
extern const uint16_t gReverbWorkAreaBaseAddrs[SPU_REV_MODE_MAX];

// New for this plugin: the names of each of the reverb effects
extern const char* const gReverbModeNames[SPU_REV_MODE_MAX];

END_NAMESPACE(SpuReverbPresets)
