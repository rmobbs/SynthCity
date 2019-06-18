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
protected:
  uint32 samplerFrequency = 0;
  uint32 frequency = 0;
  float beatLength = 0;
public:
  SynthSound(const std::string& name, uint32 samplerFrequency, uint32 frequency, uint32 duration)
    : Sound(name)
    , samplerFrequency(samplerFrequency)
    , frequency(frequency)
    , beatLength(duration) {

  }
};

class SinusSynthSound : public SynthSound {
private:
  float radians = 0;
  float radstep = 0;

public:
  
  SinusSynthSound(uint32 samplerFrequency, uint32 frequency, uint32 duration);
  uint8 getSamplesForFrame(int16* samples, uint8 channels, uint32 frame) override;
};