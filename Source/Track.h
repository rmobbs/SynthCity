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

  Patch* patch;

public:
  Track();
  Track(const ReadSerializer& serializer);
  ~Track();

  void AddNotes(uint32 noteCount, uint8 noteValue = 0);
  void SetNoteCount(uint32 noteCount, uint8 noteValue = 0);
  void SetNote(uint32 noteIndex, uint8 noteValue);

  inline const std::string& GetColorScheme(void) const {
    return colorScheme;
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

  inline const Patch* GetPatch() const {
    return patch;
  }
};

class DialogTrack : public Dialog {
protected:
  std::string trackName;
  std::string soundName;
  Dialog* subDialog = nullptr;
public:
  DialogTrack() = default;
  ~DialogTrack();

  void Open() override;
  bool Render() override;

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

