#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"

#include <string>

using SoundHandle = uint32;
static constexpr SoundHandle kInvalidSoundHandle = 0xFFFFFFFF;

class SoundInstance {
public:
  SoundHandle sound = kInvalidSoundHandle;
};

class Sound {
protected:
  std::string className;

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

  virtual SoundInstance* CreateInstance() = 0;
  virtual uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, SoundInstance* instance) = 0;

  virtual bool SerializeWrite(const WriteSerializer& serializer) = 0;
  virtual bool SerializeRead(const ReadSerializer& serializer) = 0;
};

