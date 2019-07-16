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
  SynthSound(const std::string& className, uint32 frequency, uint32 durationNum, uint32 durationDen)
    : Sound(className)
    , frequency(frequency)
    , durationNum(durationNum)
    , durationDen(durationDen) {

  }

  SynthSound(std::string className, const ReadSerializer& serializer);

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

class DialogSynthSound : public Dialog {
public:
  bool Render() override;
  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

class SineSynthVoice : public Voice {
public:
  float radians = 0;
  float radstep = 0;
  uint32 duration = 0; // Has to be calculated at Voice creation time due to reliance on current BPM
};

class SineSynthSound : public SynthSound {
public:
  SineSynthSound(uint32 frequency, uint32 durationNum, uint32 durationDen);
  SineSynthSound(const ReadSerializer& serializer);

  Voice* CreateVoice() override;
  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voice) override;
};
