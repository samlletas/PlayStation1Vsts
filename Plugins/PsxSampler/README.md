# PsxSampler
A sampler type instrument which emulates the sound of the PlayStation 1 SPU, including its unique sample interpolation and volume envelopes. Loads a sound file in the PlayStation 1 .VAG format and uses that PSX-ADPCM encoded audio as the basis for the sampler's sound.

## Limitations
- This plugin must be run at a sample rate of 44.1 KHz for correct operation, as per the sample rate of the original PlayStation's SPU.
- This plugin provides a maximum of 24 voices of polyphony, as per the PlayStation 1 SPU. This should be plenty for most uses though!

## Functionality - Sample
- **Save**: Save the currently loaded sound file to a .VAG file. Useful for extracting the current sound back out of the instrument. Note: the current sample rate is saved in the output .VAG file, even if it was modified from what it was originally.
- **Load**: Load a sound sample from a PlayStation 1 .VAG file.
- **Sample Rate**: Manually edit this field to change the sample rate of the loaded .VAG file. This action will effectively shift the pitch of the sample when performed.
- **Base Note**: This is provided as a convenience for the purposes of PlayStation Doom's music sequencer system (which uses this field) and is an alternate means to specify the sample rate. Expresses the sample rate in terms of a MIDI note; when the sample rate is 22,050Hz it will be '60', when 11,025Hz it will be '72' and when 44,100Hz it will be '48' and so on. Each doubling or halving of frequency will raise the note down or up one octave (12 notes) respectively.

## Functionality - Sample Info
- **Length (samples)**: How many samples are in the currently loaded sound.
- **Length (blocks)**: How many PSX ADPCM blocks are in the currently loaded sound. Multiply this length by '16' to obtain the size of the audio data, and by '28' to obtain the number of samples, since there are 28 samples per ADPCM block.
- **Loop Start Sample**: Information display: if the sound is looped, which sample the loop starts on (inclusive), otherwise 0.
- **Loop End Sample**: Information display: if the sound is looped, which sample the loop ends on (exclusive), otherwise 0.

## Functionality - Track
- **Volume**: Master volume multiplier for the instrument, 0-127. A value of 127 is full volume. This setting should obey MIDI channel volume control messages also.
- **Pan**: Master pan setting for the instrument, 0-127. A value of 64 is center, 0 is left, 127 is right. This setting should obey MIDI pan control messages also.
- **Pitchstep Up**: The range of the pitch bend wheel (in notes/semitones) when pitch bending up. A value of 1 = 1 semitone, and 12 = 1 octave.
- **Pitchstep Down**: The range of the pitch bend wheel (in notes/semitones) when pitch bending down. A value of 1 = 1 semitone, and 12 = 1 octave.

## Functionality - Envelope
Note: some more in-depth details about the PlayStation SPU's ADSR envelope can be found here: http://problemkaputt.de/psx-spx.htm#spuvolumeandadsrgenerator

- **Attack Step**: Affects how long the attack portion of the envelope lasts; lower values mean a faster attack.
- **Attack Shift**: Affects the scaling of the attack step and how long the attack portion of the envelope lasts; lower values mean a faster attack.
- **Attack Is Exp.**: If set then attack ramp up is exponential (and curved) rather than linear.
- **Decay Shift**: Affects how long the decay portion of the envelope lasts. Lower values mean a faster decay.
- **Sustain Level**: At what envelope level do we go from the decay phase into the sustain phase. Lower values mean a lower envelope/volume level to begin the transition at.
- **Sustain Step**: How much to step the envelope in the sustain phase. The direction of this step depends on whether the sustain direction is 'increase' or 'decrease'. Higher values mean less of a step.
- **Sustain Shift**: Affects the scaling of the sustain envelope phase step. Lower values mean a bigger step amount.
- **Sustain Dec.**: Direction of the sustain envelope phase step. If 'Yes' then the sustain envelope is decreased over time. If 'No' then it increases.
- **Sustain Is Exp**: Whether the sustain portion of the envelope increases or decreases linearly or exponentially (curved).
- **Release Shift**: Affects how long the release portion of the envelope lasts. Lower values mean a faster release.
- **Release Is Exp.**: If set then the release phase of the envelope is exponential (curved) rather than linear.
