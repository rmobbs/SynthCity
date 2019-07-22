#pragma once

#include "Sound.h"
#include "Dialog.h"

class SynthSound : public Sound {
public:
  static constexpr uint32 kDefaultFrequency = 1000; // MHz
  static constexpr uint32 kDefaultBeatsPerBar = 1; // Numerator of time sig
  static constexpr uint32 kDefaultNotesPerBeat = 4; // Denominator of time sig
protected:
  uint32 frequency = 0;

  // Seems easier to understand to express the duration of a synthesized sound as
  // the native note unit time value and count of those units, e.g. 1/4, 4/4
  uint32 durationNum = 0;
  uint32 durationDen = 0;
public:
  SynthSound(const std::string& className);
  SynthSound(const std::string& className, uint32 frequency, uint32 durationNum, uint32 durationDen);
  SynthSound(const std::string& className, const ReadSerializer& serializer);

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

class SineSynthSoundInstance : public SoundInstance {
public:
  float radians = 0;
  float radstep = 0;
  uint32 duration = 0; // Has to be calculated at instance creation time due to reliance on current BPM

  using SoundInstance::SoundInstance;
};

class SineSynthSound : public SynthSound {
public:
  SineSynthSound();
  SineSynthSound(const SineSynthSound& that);
  SineSynthSound(uint32 frequency, uint32 durationNum, uint32 durationDen);
  SineSynthSound(const ReadSerializer& serializer);

  Sound* Clone() override;

  SoundInstance* CreateInstance() override;
  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, SoundInstance* instance) override;

  void RenderDialog() override;
};
