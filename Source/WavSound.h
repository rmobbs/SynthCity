#pragma once

#include "Sound.h"
#include "Dialog.h"
#include <vector>

class WavData;
class WavSound : public Sound {
public:
  static constexpr const char* kFileNameTag = "filename";
protected:
  WavData* wavData = nullptr;

public:
  WavSound();
  WavSound(const WavSound& that);
  WavSound(const std::string& fileName);
  WavSound(const ReadSerializer& serializer);

  Sound* Clone() override;

  SoundInstance* CreateInstance() override;

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;

  inline const WavData* GetWavData() const {
    return wavData;
  }
  void SetWavData(WavData* newWavData);

  void RenderDialog() override;
};

