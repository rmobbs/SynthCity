#pragma once

#include "BaseTypes.h"

#include <string>

using SoundHandle = uint32;
static constexpr SoundHandle kInvalidSoundHandle = 0xFFFFFFFF;

// Voice is a playing instance of a sound
class Voice {
public:
  SoundHandle sound = kInvalidSoundHandle;

  int	position = 0;
  // 8:24 fixed point
  float	lvol = 0;
  float	rvol = 0;
  // 16:16 fixed point
  float	decay = 0;
};

// Sound is the invariant class; it stores the data/algorithm for generating a sound
class Sound {
protected:
  std::string name;

public:
  Sound(const std::string& name)
    : name(name) {
  }

  virtual Voice* CreateVoice();
  virtual uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voice) = 0;
};


