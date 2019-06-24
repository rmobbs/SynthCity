#pragma once

#include "Sound.h"

class SynthSound : public Sound {
protected:
  uint32 frequency = 0;
  uint32 duration = 0;
public:
  SynthSound(const std::string& name, uint32 frequency, uint32 duration)
    : Sound(name)
    , frequency(frequency)
    , duration(duration) {

  }

  SynthSound(const ReadSerializer& serializer);

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
  DECLARE_FACTORY_CLASS(SineSynthSound);

  using SynthSound::SynthSound;

  SineSynthSound(uint32 frequency, uint32 duration);
  Voice* CreateVoice() override;
  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voice) override;
};

#define REGISTER_SYNTH_SOUND(SoundClass, SoundDesc) \
  REGISTER_FACTORY_CLASS(SoundFactory, SoundClass, typeid(SynthSound).hash_code(), SoundDesc)
