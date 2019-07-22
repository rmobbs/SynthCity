#pragma once

#include "SerializeFwd.h"
#include <string>
#include <vector>

// A patch is a combination of sounds and processes that operate on those sounds
class Process;
class Sound;
class Patch {
public:
  std::vector<Process*> processes;
  std::vector<Sound*> sounds;

  Patch(const Patch& that);

  Patch(const std::vector<Process*>& processes, const std::vector<Sound*>& sounds)
    : processes(processes)
    , sounds(sounds) {

  }

  Patch() = default;
  Patch(const ReadSerializer& serializer);
  ~Patch();

  bool SerializeWrite(const WriteSerializer& serializer);
  bool SerializeRead(const ReadSerializer& serializer);

  void RenderDialog();
};


