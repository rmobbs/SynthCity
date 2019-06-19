#pragma once

#include "Sound.h"
#include <vector>

class WavSound : public Sound {
protected:
  std::vector<uint8> data;
  uint32 frequency = 0;
  uint32 channels = 0;
public:
  WavSound(const std::string& soundName);

  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voice) override;

  uint32 getFrequency() const {
    return frequency;
  }
};