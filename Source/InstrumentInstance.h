#pragma once

#include "BaseTypes.h"
#include <map>
#include <list>
#include <functional>

class Instrument;

class Note {
protected:
  uint32 beatIndex = kInvalidUint32;
  uint32 gameIndex = kInvalidUint32;

public:
  Note() = default;

  inline Note(uint32 beatIndex, uint32 gameIndex)
    : beatIndex(beatIndex)
    , gameIndex(gameIndex) {
  }

  inline uint32 GetBeatIndex() const {
    return beatIndex;
  }
  inline void SetBeatIndex(uint32 beatIndex) {
    this->beatIndex = beatIndex;
  }

  inline uint32 GetGameIndex() const {
    return gameIndex;
  }
  inline void SetGameIndex(uint32 gameIndex) {
    this->gameIndex = gameIndex;
  }
};

class TrackInstance {
public:
  class GuiNote {
  public:
    Note* note = nullptr;
    std::string uniqueGuiId;

    GuiNote();
  };

  std::string uniqueGuiIdHamburgerMenu;
  std::string uniqueGuiIdPropertiesPop;
  std::string uniqueGuiIdTrackButton;

  // Sparse list of notes
  std::list<Note> noteList;

  // Beat cells and unique GUI IDs
  std::vector<GuiNote> noteVector;

  bool mute = false;

  TrackInstance(uint32 trackId);
};

class InstrumentInstance {
private:
  void EnsureTrackNotes(TrackInstance& trackInstance, uint32 trackId, uint32 noteCount);

public:
  Instrument* instrument = nullptr;
  std::map<uint32, TrackInstance> trackInstances;
  std::string uniqueGuiIdName;
  std::string uniqueGuiIdHamburgerMenu;
  std::string uniqueGuiIdPropertiesPop;
  std::string name;

  InstrumentInstance(Instrument* instrument);
  ~InstrumentInstance();

  Note* AddNote(uint32 trackId, uint32 beatIndex, int32 gameIndex);
  void RemoveNote(uint32 trackId, uint32 beatIndex);
  void EnsureNotes(uint32 noteCount);
  void SetNoteGameIndex(uint32 trackId, uint32 beatIndex, int32 gameIndex);
  void SetTrackMute(uint32 trackId, bool mute);

  inline void SetName(std::string newName) {
    name = newName;
  }

  inline std::string GetName() const {
    return name;
  }
};
