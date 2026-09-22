# AChChid for Monsoon

Alternative firmware for the STM32F4 Monsoon/Clouds hardware. It uses the
Braids macro oscillator through an AChChid-style resonant low-pass filter and
decay envelope.

| Monsoon control | AChChid function |
| --- | --- |
| POS slider / CV | Cutoff |
| DENS slider / CV | Resonance |
| SIZE slider / CV | Envelope modulation |
| TEXT slider / CV | Decay |
| WET knob / CV | Braids model |
| STEREO knob | Braids timbre |
| FEEDBACK knob | Braids color |
| REVERB knob | Accent depth |
| TUNE knob + V/OCT | Pitch |
| TRIG | Note trigger |
| FREEZE high at TRIG | Accent that note |

The stereo inputs and input-gain control are unused. Audio is dual-mono on L
and R out. `FREEZE` is read only on a `TRIG` rising edge: it is an accent gate,
not an accent-CV input.

## Build and install

From the repository root, provide the original ARM 4.8 toolchain then run:

```sh
make -f achchid/makefile wav
```

This creates `build/achchid/achchid.wav`, which is loaded with the normal
Clouds/Monsoon audio bootloader:

1. Connect a mono output from the playback interface to Monsoon's **left audio
   input**. Disable EQ, effects and all volume normalisation; use 48 kHz.
2. Power the case down. Hold Monsoon's **panel Freeze button** (not the Freeze
   input jack) while powering the case back on, then release it once the Freeze
   LED pulses. This is the bootloader waiting state.
3. Start the WAV at low interface volume and raise it only until the transfer
   is reliable. The status LEDs show reception; blue indicates flash writing.
4. Let the 112-second WAV play to the end. The module then starts AChChid.

The bootloader remains intact, so the stock Monsoon firmware can later be
restored with its own audio-update WAV.
