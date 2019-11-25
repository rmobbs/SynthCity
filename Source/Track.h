#pragma once

#include "Dialog.h"
#include <string>
#include <functional>
#include <map>

class Sound;
class Patch;
class Instrument;

class Track {
protected:
  Instrument* instrument = nullptr;
  std::string name;
  std::string colorScheme;
  float volume = 1.0f;
  uint32 loadIndex = kInvalidUint32; // @DEPRECATE For version 1 songs, will be deprecated in version 3
  uint32 uniqueId = kInvalidUint32;

  Patch* patch = nullptr;

public:
  Track(Instrument* instrument);
  Track(const Track& that);
  Track(const std::string& name);
  Track(const ReadSerializer& serializer);
  ~Track();

  inline const std::string& GetColorScheme(void) const {
    return colorScheme;
  }
  inline void SetColorScheme(std::string newColorScheme) {
    colorScheme = newColorScheme;
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

  inline void SetVolume(float newVolume) {
    volume = newVolume;
  }
  inline float GetVolume() {
    return volume;
  }
  void SetUniqueId(uint32 uniqueId) {
    this->uniqueId = uniqueId;
  }
  inline uint32 GetUniqueId() const {
    return uniqueId;
  }
  inline void SetInstrument(Instrument* newInstrument) {
    instrument = newInstrument;
  }
  inline Instrument* GetInstrument() const {
    return instrument;
  }

  // @DEPRECATE For version 1 songs, will be deprecated in version 3
  inline void SetLoadIndex(uint32 loadIndex) {
    this->loadIndex = loadIndex;
  }
  // @DEPRECATE For version 1 songs, will be deprecated in version 3
  inline uint32 GetLoadIndex() const {
    return loadIndex;
  }
};

