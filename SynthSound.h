#pragma once

#include "Sound.h"
#include "ClassFactory.h"

struct SynthSoundInfo {
  DECLARE_CLASS_INFO;

  std::string description;
};

DECLARE_CLASS_FACTORY(SynthSoundInfoFactory, SynthSoundInfo);

#define REGISTER_SYNTH_SOUND(...) FACTORY_REGISTER(SynthSoundInfoFactory, SynthSoundInfo, SynthSoundInfo(__VA_ARGS__))

class SynthSound : public Sound {
private:
  uint32 frequency = 0;
  float beatLength = 0;
  uint32 samplerFrequency = 0;
public:
  SynthSound(const std::string& name, uint32 samplerFrequency, uint32 frequency)
    : Sound(name)
    , samplerFrequency(samplerFrequency)
    , frequency(frequency) {

  }
};

class SinusSynthSound : public SynthSound {
private:
  float radians = 0;
  float radstep = 0;

public:
  
  SinusSynthSound(uint32 samplerFrequency, uint32 frequency);
  uint8 getSamplesForFrame(uint16* samples, uint8 channels, uint32 frame) override;
};