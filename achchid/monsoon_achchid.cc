#include "achchid/achchid.h"
#include "braids/quantizer.h"

#include <cmath>

#include "clouds/drivers/adc.h"
#include "clouds/drivers/codec.h"
#include "clouds/drivers/gate_input.h"
#include "clouds/drivers/leds.h"
#include "clouds/drivers/system.h"
#include "clouds/drivers/version.h"

using achchid::Parameters;
using achchid::Voice;
using clouds::Adc;
using clouds::Codec;
using clouds::GateInput;
using clouds::Leds;

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
Codec codec;
float controls[clouds::ADC_CHANNEL_LAST];

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
  voice.SetParameters(parameters);
  if (gates.trigger_rising_edge()) {
    voice.Strike(parameters, gates.freeze());
  }
  int16_t mono[32];
  voice.Render(mono, size);
  for (size_t i = 0; i < size; ++i) {
    output[i].l = mono[i];
    output[i].r = mono[i];
  }
  leds.set_freeze(gates.freeze());
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
}
}

int main(void) {
  clouds::System system;
  clouds::Version::Init();
  system.Init(true);
  adc.Init();
  gates.Init();
  leds.Init();
  quantizer.Init();
  voice.Init();

  if (!codec.Init(!clouds::Version::revised(), 96000) || !codec.Start(32, &FillBuffer)) {
    while (1);
  }
  system.StartTimers();
  while (1);
}
