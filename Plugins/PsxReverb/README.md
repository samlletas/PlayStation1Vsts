# PsxReverb
A DSP plugin which emulates the reverb effects of the PlayStation 1's SPU. Provides the exact same reverb presets as the PlayStation 1 'PsyQ' SDK, including 'Room', 'Hall' and 'Space' reverb effects.

Additionally (for advanced users) it is possible to program new effects. All the hardware registers in the SPU which determine how reverb operates are exposed by this plugin.

## Limitations:
- This plugin must be run at a sample rate of 44.1 KHz for correct operation, as per the sample rate of the original PlayStation's SPU.

## Usage - Regular Options:
- **Choose preset**: use this option to choose one of the available presets. Note: after de-serializing previously saved VST state in a DAW this chooser will appear to have no choice, even if you loaded a preset before. This is normal and does not mean the rest of your settings (or adjustments) for your chosen preset were lost. All VST state is saved, apart from this chooser.
- **Reverb L/R-Vol**: controls the 'depth' of the reverb effect on the left and right channels. Higher values mean more reverb added to the mix.
- **Input L/R-Vol**: controls the volume of input passing into the reverb unit on the left and right channels. Normally you would leave this alone, but it may be useful to avoid clipping in some cases.
- **Master L/R-Vol**: controls the volume of output (dry input + reverb) passing from the reverb unit on the left and right channels.

## Usage - Advanced Options:
Detailed information on most of these settings and how they affect reverb can be in the NO$PSX specs: http://problemkaputt.de/psx-spx.htm#spureverbregisters

On a basic level there a couple of types of settings here:
- **WA Base Addr**: The reverb 'working area' base address. This is the address in SPU RAM (divided by 8 bytes) after which all the RAM of the SPU is dedicated to storing previous sound samples for the reverb effect. Higher values mean a smaller working area, and the maximum value of 65535 means that the working area size is just 8 bytes. How big this working area needs to be depends on the other reverb 'offset' and 'address' parameters. Basically, it needs to be sized by how far back in time the reverb effect needs to sample. If the working area is too small for the reverb settings used, then distortion and feedback loops may be noticed.
- **Volume levels**: Reflection volumes and comb filter volume levels can be adjusted. Input volume into the reverb computations can also be adjusted.
- **Offset and address values**: These values control how far back in time various parts of the reverb formula are sampling and impact the required reverb working area.
