#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"

#include <string>

class SoundInstance {
public:
  class Sound* sound = nullptr;

  SoundInstance(Sound* sound)
    : sound(sound) {

  }

  virtual uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame) = 0;
};

class Sound {
protected:
  std::string className;
  float duration = 0.0f;

public:
  Sound(std::string className)
    : className(className) {
  }

  virtual ~Sound() {

  }

  // Seriously, Windows ...
  inline const std::string& GetSoundClassName() const {
    return className;
  }

  virtual Sound* Clone() = 0;

  virtual SoundInstance* CreateInstance() = 0;

  virtual bool SerializeWrite(const WriteSerializer& serializer) = 0;
  virtual bool SerializeRead(const ReadSerializer& serializer) = 0;

  virtual void RenderDialog() = 0;

  inline float GetDuration() const {
    return duration;
  }
};

