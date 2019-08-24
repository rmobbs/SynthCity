#pragma once

#include "Dialog.h"
#include <string>
#include <functional>
#include <map>

class Sound;
class Patch;

class Track {
protected:
  std::string name;
  std::string colorScheme;
  bool mute = false;
  float volume = 1.0f;

  Patch* patch = nullptr;

public:
  Track(const Track& that);
  Track(const std::string& name);
  Track(const ReadSerializer& serializer);
  ~Track();

  inline const std::string& GetColorScheme(void) const {
    return colorScheme;
  }
  inline void SetName(const std::string& newName) {
    name = newName;
  }
  inline const std::string& GetName(void) const {
    return name;
  }

  bool SerializeWrite(const WriteSerializer& serializer);
  bool SerializeRead(const ReadSerializer& serializer);

  inline Patch* GetPatch() {
    return patch;
  }
  void SetPatch(Patch* newPatch);

  inline void SetMute(bool shouldMute) {
    mute = shouldMute;
  }
  inline bool GetMute() {
    return mute;
  }
  inline void SetVolume(float newVolume) {
    volume = newVolume;
  }
  inline float GetVolume() {
    return volume;
  }
};

