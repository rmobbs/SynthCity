#pragma once

#include "Sound.h"
#include "Dialog.h"

class SynthSound : public Sound {
public:
  static constexpr uint32 kMinFrequency = 20;
  static constexpr uint32 kMaxFrequency = 20000;
  static constexpr uint32 kDefaultFrequency = 1000;
protected:
  uint32 frequency = kDefaultFrequency;

public:
  SynthSound(const SynthSound& that);
  SynthSound(const std::string& className);
  SynthSound(const std::string& className, uint32 frequency, float duration);
  SynthSound(const std::string& className, const ReadSerializer& serializer);

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;

  inline uint32 GetFrequency() const {
    return frequency;
  }
};

