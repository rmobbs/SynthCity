#pragma once

#include "BaseTypes.h"

#include <string>

class Sound {
protected:
  std::string name;

public:
  Sound(const std::string& name)
    : name(name) {
  }

  // This is uint16 only because of how the mixer is implemented ... need to look into a more generic method
  virtual uint8 getSamplesForFrame(float* samples, uint8 channels, uint32 frame) = 0;
};