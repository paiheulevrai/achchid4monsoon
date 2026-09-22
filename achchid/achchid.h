// AChChid voice for the STM32F4 Clouds/Monsoon hardware.
#ifndef ACHCHID_ACHCHID_H_
#define ACHCHID_ACHCHID_H_

#include "braids/macro_oscillator.h"

namespace achchid {

struct Parameters {
  int16_t pitch;
  uint8_t model;
  int16_t timbre;
  int16_t color;
  float cutoff;
  float resonance;
  float env_mod;
  float decay;
  float accent;
};

class Voice {
 public:
  void Init();
  void SetParameters(const Parameters& parameters);
  void Strike(const Parameters& parameters, bool accent);
  void Render(int16_t* output, size_t size);
  void RenderBraidsRaw(int16_t* output, size_t size);

 private:
  braids::MacroOscillator oscillator_;
  Parameters parameters_;
  float envelope_;
  float envelope_decay_;
  float amp_envelope_;
  float amp_decay_;
  float accent_rc_;
  float accent_rc_coefficient_;
  float feedback_x_;
  float feedback_y_;
  float ladder_1_;
  float ladder_2_;
  float ladder_3_;
  float ladder_4_;
  bool accented_;
  int16_t buffer_[32];
  uint8_t sync_[32];
};

}  // namespace achchid

#endif  // ACHCHID_ACHCHID_H_
