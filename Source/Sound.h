#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"

#include <string>

// A SoundInstance is a playing instance of a sound
class SoundInstance {
protected:
  uint32 soundHash = 0;
public:
  class Sound* sound = nullptr;

  SoundInstance() = default;
  SoundInstance(Sound* sound);

  virtual ~SoundInstance() {

  }

  inline uint32 GetSoundHash() const {
    return soundHash;
  }

  virtual uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame) = 0;
};

// A Sound is a collection of data/parameters that generates a sound
class Sound {
protected:
  std::string className;
  uint32 classHash = 0;
  float duration = 0.0f;
public:
  Sound(const Sound& that);
  Sound(const std::string& className);

  virtual ~Sound() {

  }

  // Seriously, Windows ...
  inline const std::string& GetSoundClassName() const {
    return className;
  }

  inline uint32 GetClassHash() const {
    return classHash;
  }

  virtual Sound* Clone() = 0;

  virtual bool SerializeWrite(const WriteSerializer& serializer) = 0;
  virtual bool SerializeRead(const ReadSerializer& serializer) = 0;

  virtual void RenderDialog() = 0;

  inline float GetDuration() const {
    return duration;
  }
};

