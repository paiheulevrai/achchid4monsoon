# AChChid for Monsoon

> **MVP / work in progress.** The firmware flashes and produces sound, but it
> is not a finished instrument yet. In particular, Braids has not been
> validated across every oscillator model, timbre, colour, pitch range, or
> V/OCT tracking scenario. Use it for testing and experimentation, not a live
> set you cannot recover from.

AChChid for Monsoon is an alternative firmware for **Monsoon**, the Eurorack
clone/expanded version of Mutable Instruments Clouds. It ports the AChChid
voice from [ChooChooTracker](https://github.com/paiheulevrai/Choochootracker) to
the Clouds/Monsoon STM32F4 platform.

The idea is simple and a little unreasonable in the best way: a TB-303-style
voice whose oscillator is the Mutable Instruments **Braids** macro oscillator.
Braids is the sole sound source; its output is sent through the AChChid/Open303
filter and envelope path. The goal is to keep the strong character of the
Open303 filter and envelopes while making the oscillator selectable and
mutable in the Braids way.

## Hardware

- Monsoon or compatible Clouds hardware using the stock Monsoon bootloader.
- Firmware updates are installed as an audio WAV through the left audio input.
- Audio output is dual-mono on OUT L and OUT R.

## Controls

| Monsoon control | AChChid function |
| --- | --- |
| POS slider / CV | Filter cutoff |
| DENS slider / CV | Filter resonance |
| SIZE slider / CV | Filter envelope modulation |
| TEXT slider / CV | Envelope decay |
| WET knob / CV | Braids model |
| STEREO knob | Braids timbre |
| FEEDBACK knob | Braids colour |
| REVERB knob | Accent depth |
| TUNE knob + V/OCT | Pitch |
| TRIG | Note trigger |
| FREEZE high at the TRIG edge | Accent for that note |

FREEZE is an accent gate, not an accent-CV input. The audio inputs and input
gain are unused by this firmware.

## Current status

Working enough to test:

- Audio bootloader installation.
- Audio output.
- TRIG reception.
- V/OCT and panel control routing.
- 96 kHz audio operation, required by the embedded Braids core.

Still to validate before calling this a release:

- Every Braids oscillator model and its timbre/colour controls.
- Accurate pitch and V/OCT behaviour over the useful musical range.
- Full Open303 response under resonance, accent, fast retriggering, and the
  complete control range.
- Long-term DSP stability and CPU headroom on real Monsoon hardware.

## Install

Download `achchid.wav` from the latest GitHub release.

1. Connect a clean mono output from your playback interface to Monsoon's left
   audio input. Disable EQ, effects, and volume normalisation.
2. Hold the **panel Freeze button** while powering on the case. Release it
   when the Freeze LED pulses: the bootloader is waiting for audio.
3. Play the WAV at 48 kHz. Start with a low interface level and raise it only
   as far as needed for a reliable transfer.
4. Let the file finish. The bootloader remains installed, so stock Monsoon
   firmware can always be restored using its normal update WAV.

## Build

See [`achchid/README.md`](achchid/README.md). The build generates
`build/achchid/achchid.wav`.

## Credits and licence

This project builds on Mutable Instruments Clouds and Braids, the Open303
engine, the Monsoon firmware work in the original Eurorack fork, and the
AChChid voice from ChooChooTracker.

STM32F firmware code follows the upstream MIT licence; retain the respective
upstream notices when redistributing. Hardware material in the upstream
Eurorack project is CC-BY-SA 3.0.
