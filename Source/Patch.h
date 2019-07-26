#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include <string>
#include <vector>

// A patch is a combination of sounds and processes that operate on those sounds
class Process;
class Sound;
class Patch {
public:
  static constexpr uint32 kMaxSounds = 8;
  static constexpr uint32 kMaxProcesses = 8;
protected:
  float soundDuration = 0.0f;
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

  void AddSound(Sound* sound);
  void AddProcess(Process* process);
  void RemoveSound(Sound* sound);
  void RemoveProcess(Process* process);
  void UpdateDuration();

  inline float GetSoundDuration() const {
    return soundDuration;
  }
};


