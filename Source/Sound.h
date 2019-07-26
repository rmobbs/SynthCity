#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"

#include <string>

// A SoundInstance is a playing instance of a sound
class SoundInstance {
public:
  class Sound* sound = nullptr;

  SoundInstance() = default;

  SoundInstance(Sound* sound)
    : sound(sound) {

  }
  virtual ~SoundInstance() {
    int q = 5;
  }

  virtual uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame) = 0;
};

// A Sound is a collection of data/parameters that generates a sound
class Sound {
protected:
  std::string className;
  float duration = 0.0f;

public:
  Sound(std::string className)
    : className(className) {
  }

  virtual ~Sound() {
    int q = 5;
  }

  // Seriously, Windows ...
  inline const std::string& GetSoundClassName() const {
    return className;
  }

  virtual Sound* Clone() = 0;

  virtual bool SerializeWrite(const WriteSerializer& serializer) = 0;
  virtual bool SerializeRead(const ReadSerializer& serializer) = 0;

  virtual void RenderDialog() = 0;

  inline float GetDuration() const {
    return duration;
  }
};

