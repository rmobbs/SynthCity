#pragma once

#include "Sound.h"
#include <vector>

class WavSound : public Sound {
protected:
  std::string fileName;
  uint32 frequency = 0;
  uint32 channels = 0;
  float decay = 0.0f;
  std::vector<uint8> data;

  bool LoadWav(const std::string& fileName);

public:
  DECLARE_FACTORY_CLASS(WavSound);

  WavSound(const ReadSerializer& serializer);
  WavSound(const std::string& fileName);

  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voice) override;

  uint32 getFrequency() const {
    return frequency;
  }

  Voice* CreateVoice() override;
  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};