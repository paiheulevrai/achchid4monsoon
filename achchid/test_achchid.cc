#include <cassert>
#include <cstdio>
#include <cstdint>

#include "achchid/achchid.h"
#include "braids/quantizer.h"

braids::Quantizer quantizer;

int main() {
  achchid::Voice voice;
  achchid::Parameters parameters = { 60 << 7, 0, 16384, 16384,
      0.35f, 0.5f, 0.75f, 0.25f, 1.0f };
  int16_t output[32];
  quantizer.Init();
  voice.Init();
  voice.Strike(parameters, true);
  int64_t energy = 0;
  int16_t peak = 0;
  for (size_t block = 0; block < 100; ++block) {
    voice.Render(output, 32);
    for (size_t i = 0; i < 32; ++i) {
      int16_t absolute = output[i] < 0 ? -output[i] : output[i];
      energy += absolute;
      if (absolute > peak) peak = absolute;
    }
  }
  std::printf("Open303 energy=%lld peak=%d\n", static_cast<long long>(energy), peak);
  assert(energy > 100000);
  voice.RenderBraidsRaw(output, 32);
  energy = 0;
  for (size_t i = 0; i < 32; ++i) energy += output[i] < 0 ? -output[i] : output[i];
  assert(energy > 0);
}
