#pragma once

#include "Sound.h"
#include "Dialog.h"
#include <vector>

class WavSound : public Sound {
public:
  static constexpr const char* kFileNameTag = "filename";
  static constexpr const char* kDecayTag = "decay";
protected:
  std::string fileName;
  uint32 frequency = 0;
  uint32 channels = 0;
  float decay = 0.0f;
  std::vector<uint8> data;

  bool LoadWav(const std::string& fileName);

public:
  WavSound(const std::string& fileName);
  WavSound(const ReadSerializer& serializer);

  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, SoundInstance* instance) override;

  uint32 getFrequency() const {
    return frequency;
  }

  SoundInstance* CreateInstance() override;
  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

class DialogWavSound : public Dialog {
protected:
  std::string fileName;
public:
  bool Render() override;
  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

