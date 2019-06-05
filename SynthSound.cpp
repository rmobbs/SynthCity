#include "SynthSound.h"
#define _USE_MATH_DEFINES
#include <math.h>

SinusSynthSound::SinusSynthSound(const std::string& soundName, uint32 samplerFrequency, uint32 frequency)
  : SynthSound(soundName, samplerFrequency, frequency) {
  this->radstep = static_cast<float>((2.0 * M_PI * frequency) / static_cast<double>(samplerFrequency));
}

uint8 SinusSynthSound::getSamplesForFrame(uint16* samples, uint8 channels, uint32 frame) {
  this->radians += this->radstep; // * speed
  while (this->radians > (2.0 * M_PI)) {
    this->radians -= 2.0 * M_PI;
  }

  float s = sinf(this->radians);
  for (uint8 channel = 0; channel < channels; ++channel) {
    samples[channel] = static_cast<uint16>(s * SHRT_MAX);
  }
  return channels;
}
