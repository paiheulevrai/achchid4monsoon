#include "achchid/achchid.h"
#include "braids/quantizer.h"

#include <cmath>

#include "clouds/drivers/adc.h"
#include "clouds/drivers/codec.h"
#include "clouds/drivers/gate_input.h"
#include "clouds/drivers/leds.h"
#include "clouds/drivers/system.h"
#include "clouds/drivers/switches.h"
#include "clouds/drivers/version.h"

using achchid::Parameters;
using achchid::Voice;
using clouds::Adc;
using clouds::Codec;
using clouds::GateInput;
using clouds::Leds;
using clouds::Switches;

// Some Braids digital models share the standalone firmware's global quantizer.
braids::Quantizer quantizer;

// Newlib's compact embedded configuration expects the application to supply
// this storage when math functions set errno.
int __errno;

namespace {

Voice voice;
Adc adc;
GateInput gates;
Leds leds;
Switches switches;
Codec codec;
float controls[clouds::ADC_CHANNEL_LAST];

// Temporary hardware diagnostic: panel buttons play a self-contained,
// known-good voice without relying on the external trigger or panel settings.
int16_t active_test_pitch = 0;
bool test_active = false;
bool previous_mode_button = false;
bool previous_write_button = false;
uint16_t test_button_feedback = 0;
bool direct_dac_test = false;
bool raw_braids_test = false;
uint32_t direct_dac_phase = 0;

float Clamp(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

float ReadControl(clouds::AdcChannel channel, bool inverted = false) {
  float target = adc.float_value(channel);
  if (inverted) target = 1.0f - target;
  controls[channel] += 0.04f * (target - controls[channel]);
  return controls[channel];
}

Parameters ReadParameters() {
  Parameters p;

  // Monsoon panel: POS, DENS, SIZE and TEXT faders, including their CV jacks.
  p.cutoff = ReadControl(clouds::ADC_POSITION_POTENTIOMETER_CV, true);
  p.resonance = ReadControl(clouds::ADC_DENSITY_POTENTIOMETER_CV, true);
  p.env_mod = Clamp(ReadControl(clouds::ADC_SIZE_POTENTIOMETER) -
                    ReadControl(clouds::ADC_SIZE_CV));
  p.decay = Clamp(ReadControl(clouds::ADC_TEXTURE_POTENTIOMETER) -
                  ReadControl(clouds::ADC_TEXTURE_CV));

  // Monsoon's individual Blend knobs and CV inputs become Braids/acid controls.
  float model = Clamp(ReadControl(clouds::ADC_WET_POTENTIOMETER) -
                      ReadControl(clouds::ADC_WET_CV));
  p.model = static_cast<uint8_t>(model * 46.99f);
  p.timbre = static_cast<int16_t>(Clamp(ReadControl(
      clouds::ADC_STEREO_POTENTIOMETER_CV, true)) * 32767.0f);
  p.color = static_cast<int16_t>(Clamp(ReadControl(
      clouds::ADC_FEEDBACK_POTENTIOMETER_CV, true)) * 32767.0f);
  p.accent = Clamp(ReadControl(clouds::ADC_REVERB_POTENTIOMETER_CV, true));

  // Retain the large TUNE knob and the calibrated Clouds V/OCT input.
  float tune = (ReadControl(clouds::ADC_PITCH_POTENTIOMETER) - 0.5f) * 96.0f;
  float volts = adc.float_value(clouds::ADC_V_OCT_CV);
  // AChChid addresses Braids one octave above its incoming note, matching the
  // original engine's `(note + 12) * 128` conversion.
  p.pitch = static_cast<int16_t>((72.0f + tune + 66.67f - volts * 84.26f) * 128.0f);
  return p;
}

void FillBuffer(Codec::Frame*, Codec::Frame* output, size_t size) {
  gates.Read();
  Parameters parameters = ReadParameters();
  bool mode_button = switches.pressed_immediate(0);
  bool write_button = switches.pressed_immediate(1);
  uint8_t test_note = 0;
  if (mode_button && !previous_mode_button) {
    test_note = 60;  // C3.
  } else if (write_button && !previous_write_button) {
    test_note = 72;  // C4.
  }
  previous_mode_button = mode_button;
  previous_write_button = write_button;
  if (test_note) {
    active_test_pitch = static_cast<int16_t>(test_note << 7);
    test_active = true;
    direct_dac_test = test_note == 72;
    raw_braids_test = test_note == 60;
    test_button_feedback = 100;
  } else if (gates.trigger_rising_edge()) {
    active_test_pitch = 0;
    test_active = false;
    direct_dac_test = false;
    raw_braids_test = false;
  }
  if (test_active) {
    parameters.pitch = active_test_pitch + (12 << 7);
    parameters.model = braids::MACRO_OSC_SHAPE_CSAW;
    parameters.timbre = 16384;
    parameters.color = 16384;
    parameters.cutoff = 1.0f;
    parameters.resonance = 0.0f;
    parameters.env_mod = 0.5f;
    parameters.decay = 0.5f;
    parameters.accent = 0.0f;
  }
  voice.SetParameters(parameters);
  if (test_note) {
    voice.Strike(parameters, false);
  } else if (gates.trigger_rising_edge()) {
    voice.Strike(parameters, gates.freeze());
  }
  int16_t mono[32];
  if (raw_braids_test) {
    voice.RenderBraidsRaw(mono, size);
  } else {
    voice.Render(mono, size);
  }
  if (direct_dac_test) {
    // Temporary C4 diagnostic: bypass every synth stage and drive the DAC
    // with a 523 Hz square wave. This isolates the physical output path.
    for (size_t i = 0; i < size; ++i) {
      direct_dac_phase += 23418197;  // 523 Hz at 96 kHz.
      mono[i] = (direct_dac_phase & 0x80000000) ? 18000 : -18000;
    }
  }
  for (size_t i = 0; i < size; ++i) {
    output[i].l = mono[i];
    output[i].r = mono[i];
  }
  bool show_test_button = test_button_feedback != 0;
  if (test_button_feedback) --test_button_feedback;
  leds.set_freeze(gates.freeze() || show_test_button);
  leds.set_intensity(0, static_cast<uint8_t>(parameters.cutoff * 255.0f));
  leds.set_intensity(1, static_cast<uint8_t>(parameters.resonance * 255.0f));
  leds.set_intensity(2, static_cast<uint8_t>(parameters.env_mod * 255.0f));
  leds.set_intensity(3, static_cast<uint8_t>(parameters.decay * 255.0f));
  leds.Write();
  adc.Convert();
}

}  // namespace

extern "C" {
void NMI_Handler() { }
void HardFault_Handler() { while (1); }
void MemManage_Handler() { while (1); }
void BusFault_Handler() { while (1); }
void UsageFault_Handler() { while (1); }
void SVC_Handler() { }
void DebugMon_Handler() { }
void PendSV_Handler() { }
void SysTick_Handler() {
  switches.Debounce();
}
}

int main(void) {
  clouds::System system;
  clouds::Version::Init();
  system.Init(true);
  adc.Init();
  gates.Init();
  leds.Init();
  switches.Init();
  quantizer.Init();
  voice.Init();

  if (!codec.Init(!clouds::Version::revised(), 96000) || !codec.Start(32, &FillBuffer)) {
    while (1);
  }
  system.StartTimers();
  while (1);
}
