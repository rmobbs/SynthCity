#pragma once

#include "Dialog.h"
#include <string>
#include <functional>
#include <map>

class Sound;
class Patch;

//using Note = uint8;

class Track {
public:
  class Note {
  public:
    bool enabled = false;
    int32 fretIndex = -1;
  };
protected:
  std::string name;
  std::string colorScheme;
  std::vector<Note> notes;
  bool mute = false;
  float volume = 1.0f;

  Patch* patch = nullptr;

public:
  Track(const Track& that);
  Track(const std::string& name);
  Track(const ReadSerializer& serializer);
  ~Track();

  void AddNotes(uint32 noteCount);
  void SetNoteCount(uint32 noteCount);
  uint32 GetNoteCount() const {
    return notes.size();
  }
  void SetNote(uint32 noteIndex, const Note& note);
  void SetNotes(const std::vector<Note>& newNotes);

  inline const std::string& GetColorScheme(void) const {
    return colorScheme;
  }
  inline void SetName(const std::string& newName) {
    name = newName;
  }
  inline const std::string& GetName(void) const {
    return name;
  }
  inline const std::vector<Note>& GetNotes(void) const {
    return notes;
  }
  void ClearNotes();
  Note& GetNote(uint32 noteIndex);

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

