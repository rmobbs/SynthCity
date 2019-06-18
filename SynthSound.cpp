#include "SynthSound.h"
#define _USE_MATH_DEFINES
#include <math.h>

REGISTER_SYNTH_SOUND({ "SinusSynthSound", "Sine wave" });
SinusSynthSound::SinusSynthSound(uint32 samplerFrequency, uint32 frequency, uint32 duration)
  : SynthSound("SineSynthSound", samplerFrequency, frequency, duration) {
  this->radstep = static_cast<float>((2.0 * M_PI * frequency) / static_cast<double>(samplerFrequency));
}

uint8 SinusSynthSound::getSamplesForFrame(float* samples, uint8 channels, uint32 frame) {
  if (frame < beatLength) {
    this->radians += this->radstep; // * speed
    while (this->radians > (2.0 * M_PI)) {
      this->radians -= static_cast<float>(2.0 * M_PI);
    }

    float s = sinf(this->radians);
    for (uint8 channel = 0; channel < channels; ++channel) {
      samples[channel] = s;
    }
    return channels;
  }
  return 0;
}
