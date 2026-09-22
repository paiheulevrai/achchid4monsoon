#include "achchid/achchid.h"

#include <cmath>
#include <cstring>

namespace achchid {

namespace {
// Braids and the desktop AChChid engine both run at 96 kHz. Rendering its
// fixed-rate core at Clouds' usual 32 kHz lowers pitch by three and aliases.
const float kSampleRate = 96000.0f;
const float kOversampledRate = kSampleRate * 4.0f;
const float kPi = 3.14159265358979323846f;

float Clamp(float x, float lo, float hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}
}  // namespace

void Voice::Init() {
  oscillator_.Init();
  oscillator_.set_shape(braids::MACRO_OSC_SHAPE_CSAW);
  envelope_ = 0.0f;
  amp_envelope_ = 0.0f;
  accent_rc_ = 0.0f;
  feedback_x_ = feedback_y_ = 0.0f;
  ladder_1_ = ladder_2_ = ladder_3_ = ladder_4_ = 0.0f;
  accented_ = false;
}

void Voice::Strike(const Parameters& parameters, bool accent) {
  accented_ = accent;
  SetParameters(parameters);
  oscillator_.Strike();
  envelope_ = 1.0f;
  amp_envelope_ = 1.0f;
  accent_rc_ = 0.0f;
}

void Voice::SetParameters(const Parameters& parameters) {
  parameters_ = parameters;
  oscillator_.set_shape(static_cast<braids::MacroOscillatorShape>(parameters.model));
  oscillator_.set_pitch(parameters.pitch);
  oscillator_.set_parameters(parameters.timbre, parameters.color);
  // Open303's normal filter envelope is 200..2000 ms; an accented note uses
  // its original fixed 200 ms decay and the original 1.23 s VCA envelope.
  float milliseconds = 200.0f + parameters.decay * 1800.0f;
  if (accented_) milliseconds = 200.0f;
  envelope_decay_ = expf(-1.0f / (milliseconds * 0.001f * kSampleRate));
  amp_decay_ = expf(-1.0f / (1.230f * kSampleRate));
  accent_rc_coefficient_ = expf(-1.0f / (0.015f * kSampleRate));
}

void Voice::RenderBraidsRaw(int16_t* output, size_t size) {
  size_t offset = 0;
  while (offset < size) {
    size_t block_size = size - offset;
    if (block_size > sizeof(buffer_) / sizeof(buffer_[0])) {
      block_size = sizeof(buffer_) / sizeof(buffer_[0]);
    }
    memset(sync_, 0, block_size);
    oscillator_.Render(sync_, output + offset, block_size);
    offset += block_size;
  }
}

void Voice::Render(int16_t* output, size_t size) {
  // Open303's control path is smooth compared to the audio rate. Updating the
  // ladder coefficients every 8 samples retains the envelope shape while
  // keeping the STM32F4 inside its DMA deadline.
  float base_cutoff_hz = 200.0f * expf(
      4.6051702f * Clamp(parameters_.cutoff, 0.0f, 1.0f));
  float cutoff_curve = logf(
      Clamp(base_cutoff_hz, 313.81528f, 2394.4119f) / 313.81528f) /
      logf(2394.4119f / 313.81528f);
  float env_mod = parameters_.env_mod;
  float low_scale = 3.7739963f * env_mod + 0.7369656f;
  float high_scale = 4.1945488f * env_mod + 0.8643449f;
  float env_scaler = (1.0f - cutoff_curve) * low_scale + cutoff_curve * high_scale;
  float env_offset = 0.04829293f * cutoff_curve + 0.2943912f;
  float resonance = (1.0f - expf(-3.0f * parameters_.resonance)) /
      (1.0f - expf(-3.0f));
  float hp_a = expf(-2.0f * kPi * 150.0f / kOversampledRate);
  float b0 = 0.0f;
  float k = 0.0f;
  float gain = 0.0f;
  size_t filter_control_counter = 0;

  size_t offset = 0;
  while (offset < size) {
    size_t block_size = size - offset;
    if (block_size > sizeof(buffer_) / sizeof(buffer_[0])) {
      block_size = sizeof(buffer_) / sizeof(buffer_[0]);
    }
    memset(sync_, 0, block_size);
    oscillator_.Render(sync_, buffer_, block_size);
    for (size_t i = 0; i < block_size; ++i) {
    // This is the Open303 external-input path, in float: its decay envelope,
    // accent RC, feedback high-pass and TB-303 ladder are kept intact.
    float env = envelope_;
    envelope_ *= envelope_decay_;
    amp_envelope_ *= amp_decay_;
    accent_rc_ = env + accent_rc_coefficient_ * (accent_rc_ - env);
    float accent = accented_ ? parameters_.accent : 0.0f;
    float env_octaves = env_scaler * (env - env_offset) + accent * accent_rc_;
    if ((filter_control_counter++ & 7) == 0) {
      float cutoff_hz = base_cutoff_hz * exp2f(env_octaves);
      cutoff_hz = Clamp(cutoff_hz / 20000.0f, 0.01f, 1.0f) * 20000.0f;
      float wc = 2.0f * kPi * cutoff_hz / kOversampledRate;
      float fx = wc * 0.70710678f / (2.0f * kPi);
      b0 = (0.00045522346f + 6.1922189f * fx) /
          (1.0f + 12.358354f * fx + 4.4156345f * fx * fx);
      k = fx + 7198.6997f;
      k = fx * k - 5837.7917f;
      k = fx * k - 476.47308f;
      k = fx * k + 614.95611f;
      k = fx * k + 213.87126f;
      k = fx * k + 16.998792f;
      gain = k * 0.0588235294f;
      gain = (gain - 1.0f) * resonance + 1.0f;
      gain *= 1.0f + resonance;
      k *= resonance;
    }

    // Open303 feeds the same VCO sample into every oversampling pass; the
    // ladder state, not the previous filter output, provides its feedback.
    float input = static_cast<float>(buffer_[i]) * (-1.0f / 32768.0f);
    float sample = 0.0f;
    // The original Open303 ladder is evaluated four times per audio sample.
    for (int j = 0; j < 4; ++j) {
      float hp_x = k * ladder_4_;
      feedback_y_ = 0.5f * (1.0f + hp_a) * hp_x -
          0.5f * (1.0f + hp_a) * feedback_x_ + hp_a * feedback_y_;
      feedback_x_ = hp_x;
      float in = input - feedback_y_;
      ladder_1_ += 2.0f * b0 * (in - ladder_1_ + ladder_2_);
      ladder_2_ += b0 * (ladder_1_ - 2.0f * ladder_2_ + ladder_3_);
      ladder_3_ += b0 * (ladder_2_ - 2.0f * ladder_3_ + ladder_4_);
      ladder_4_ += b0 * (ladder_3_ - 2.0f * ladder_4_);
      sample = 2.0f * gain * ladder_4_;
    }
    float vca = amp_envelope_ + 0.45f * env + accent * 4.0f * env;
    sample *= vca;
    // Soft saturation preserves resonance peaks without codec wraparound.
    sample = sample / (1.0f + fabsf(sample));
    output[offset + i] = static_cast<int16_t>(sample * 30000.0f);
  }
    offset += block_size;
  }
}

}  // namespace achchid
