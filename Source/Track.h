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
  std::vector<uint8> notes;
  bool mute = false;
  float volume = 1.0f;

  Patch* patch = nullptr;

public:
  Track(const std::string& name);
  Track(const ReadSerializer& serializer);
  ~Track();

  void AddNotes(uint32 noteCount, uint8 noteValue = 0);
  void SetNoteCount(uint32 noteCount, uint8 noteValue = 0);
  void SetNote(uint32 noteIndex, uint8 noteValue);

  inline const std::string& GetColorScheme(void) const {
    return colorScheme;
  }
  inline void SetName(const std::string& newName) {
    name = newName;
  }
  inline const std::string& GetName(void) const {
    return name;
  }
  inline const std::vector<uint8>& GetNotes(void) const {
    return notes;
  }
  void ClearNotes();

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

