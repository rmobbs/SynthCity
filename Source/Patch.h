#pragma once

#include "SerializeFwd.h"
#include <string>

// A patch is a combination of sounds and processes that operate on those sounds
class Process;
class Sound;
class Patch {
public:
  Process* process;
  Sound* sound;

  std::string processName;
  std::string soundName;

  inline Patch(Process* process, Sound* sound)
    : process(process)
    , sound(sound) {

  }

  Patch(const ReadSerializer& serializer);
  ~Patch();

  bool SerializeWrite(const WriteSerializer& serializer);
  bool SerializeRead(const ReadSerializer& serializer);

  void RenderDialog();
};


