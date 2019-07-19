#pragma once

#include "Sound.h"
#include "Dialog.h"
#include <vector>

class WavData;
class WavSound : public Sound {
public:
  static constexpr const char* kFileNameTag = "filename";
protected:
  WavData* wavData;

public:
  WavSound(const std::string& fileName);
  WavSound(const ReadSerializer& serializer);

  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, SoundInstance* instance) override;

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

