# PlayStation 1 VST Instruments

This is a fork of the [iPlug 2 Framework](https://github.com/iplug2/iplug2) housing 2 VST instruments which enable the sound of the PlayStation 1 SPU to be replicated for classic video game music production.

One VST is an instrument which allows a PlayStation 1 ADPCM sample (.VAG file) to be played like a standard sample-based instrument, with authentic PS1 style ADSR envelope settings and SPU sample interpolation. The second VST is a reverb module which allows recreating the PS1's reverb effects.

Both instruments use SPU emulation code from the [PsyDoom](https://github.com/BodbDearg/PsyDoom) PlayStation Doom port and are mainly intended to allow for music production for that game. However they could also easily be used for other PS1 related projects.

LIMITATION: Both plugins must be used at a 44.1 KHz sample rate, as per the original PS1 hardware. Any other sample rates will result in incorrect results.
