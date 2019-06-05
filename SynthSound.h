#pragma once

#include "Sound.h"

// Interim class for synthesized sounds for eventual common functionality
class SynthSound : public Sound {
private:
  uint32 frequency = 0;
public:
  SynthSound(const std::string& name, uint32 frequency)
    : Sound(name)
    , frequency(frequency) {

  }
};

class SinusSynthSound : public SynthSound {
private:
  float radians = 0;
  float radstep = 0;
public:
  SinusSynthSound(const std::string& soundName, uint32 frequency, uint32 samplerFrequency);
  uint8 getSamplesForFrame(uint16* samples, uint8 channels, uint32 frame) override;
};