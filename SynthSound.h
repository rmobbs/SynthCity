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
  uint32 beatLength = 0;
public:
  SynthSound(const std::string& name, uint32 samplerFrequency, uint32 frequency, uint32 duration)
    : Sound(name)
    , samplerFrequency(samplerFrequency)
    , frequency(frequency)
    , beatLength(duration) {

  }

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

class SineSynthVoice : public Voice {
public:
  float radians = 0;
  float radstep = 0;
};
class SineSynthSound : public SynthSound {
public:
  
  SineSynthSound(uint32 samplerFrequency, uint32 frequency, uint32 duration);
  Voice* CreateVoice() override;
  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voice) override;
};