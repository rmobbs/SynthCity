#include "Patch.h"
#include "Logging.h"
#include "SerializeImpl.h"
#include "ProcessFactory.h"
#include "SoundFactory.h"

#include <stdexcept>

static constexpr const char* kPatchTag("patch");
static constexpr const char* kProcessesTag("processes");
static constexpr const char* kSoundsTag("sounds");

Patch::Patch(const ReadSerializer& serializer) {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Unable to serialize patch");
  }
}

bool Patch::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  if (!d.HasMember(kPatchTag) || !d[kPatchTag].IsObject()) {
    MCLOG(Error, "Missing or invalid patch section in track");
    return false;
  }

  const auto& patch = d[kPatchTag];

  // Processes
  if (!patch.HasMember(kProcessesTag) || !patch[kProcessesTag].IsArray()) {
    MCLOG(Error, "Missing or invalid process array in patch");
    return false;
  }
  const auto& processesArray = patch[kProcessesTag];
  if (processesArray.Size()) {
    // Take the first one for now
    const auto& processEntry = processesArray[0];

    // Get class
    if (!processEntry.HasMember(kClassTag) || !processEntry[kClassTag].IsString()) {
      MCLOG(Error, "No class tag for process");
      return false;
    }

    std::string className(processEntry[kClassTag].GetString());

    // Get factory
    const auto& processInfoMap = ProcessFactory::GetInfoMap();
    const auto& processInfo = processInfoMap.find(className);
    if (processInfo == processInfoMap.end()) {
      MCLOG(Error, "Invalid class tag for process");
      return false;
    }

    try {
      process = processInfo->second.factory({ processEntry });
    }
    catch (...) {
      return false;
    }
  }

  // Sounds
  if (!patch.HasMember(kSoundsTag) || !patch[kSoundsTag].IsArray()) {
    MCLOG(Error, "Missing or invalid sound array in patch");
    return false;
  }
  const auto& soundArray = patch[kSoundsTag];
  if (soundArray.Size()) {
    // Take the first one for now
    const auto& soundEntry = soundArray[0];

    // Get factory
    if (!soundEntry.HasMember(kClassTag) || !soundEntry[kClassTag].IsString()) {
      MCLOG(Error, "No class tag for sound");
      return false;
    }
    std::string className(soundEntry[kClassTag].GetString());

    const auto& soundInfoMap = SoundFactory::GetInfoMap();
    const auto& soundInfo = soundInfoMap.find(className);
    if (soundInfo == soundInfoMap.end()) {
      MCLOG(Error, "Invalid class tag for sound");
      return false;
    }

    try {
      sound = soundInfo->second.factory({ soundEntry });
    }
    catch (...) {
      return false;
    }
  }

  return true;
}

bool Patch::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;
  return true;
}

