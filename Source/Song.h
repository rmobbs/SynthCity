#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include "Globals.h"
#include <string>
#include <map>
#include <list>
#include <functional>

class Instrument;
class Track;
class Song {
public:
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

  class InstrumentInstance {
  public:
    Instrument* instrument = nullptr;
    std::map<uint32, std::list<Note>> lines;

    inline InstrumentInstance(Instrument* instrument) :
      instrument(instrument) {

    }
    ~InstrumentInstance();

    Note* AddNote(uint32 trackId, uint32 beatIndex);
    void RemoveNote(uint32 trackId, uint32 beatIndex);
    void AddTrack(Track* newTrack);
    void RemoveTrack(uint32 trackId);
  };

  static constexpr uint32 kDefaultNumMeasures = 4;
  static constexpr uint32 kDefaultBeatsPerMeasure = 4;
  static constexpr const char* kDefaultName = "Untitled";

protected:
  static std::pair<void*, std::function<void(Song::InstrumentInstance*, uint32 trackId, void*)>> onTrackAdded;
  static std::pair<void*, std::function<void(Song::InstrumentInstance*, uint32 trackId, void*)>> onTrackRemoved;

  uint32 tempo = Globals::kDefaultTempo;
  uint32 numMeasures = 0;
  uint32 beatsPerMeasure = kDefaultBeatsPerMeasure;
  uint32 minNoteValue = Globals::kDefaultMinNote;
  std::string name;

  std::list<InstrumentInstance*> instrumentInstances;

  std::function<Instrument*(std::string)> instrumentLoader;

  static Song* LoadSongMidi(std::string fileName, std::function<Instrument*(std::string)> instrumentLoader);
  static Song* LoadSongJson(std::string fileName, std::function<Instrument*(std::string)> instrumentLoader);

  std::pair<bool, std::string> SerializeReadInstrument23(const ReadSerializer& serializer);
  
public:
  Song(std::string name, uint32 tempo, uint32 numMeasures, uint32 beatsPerMeasure, uint32 minNoteValue);
  Song(const ReadSerializer& serializer, std::function<Instrument*(std::string)> instrumentLoader);
  ~Song();

  std::pair<bool, std::string> SerializeRead(const ReadSerializer& serializer);
  std::pair<bool, std::string> SerializeWrite(const WriteSerializer& serializer);

  inline const std::list<InstrumentInstance*>& GetInstruments() const {
    return instrumentInstances;
  }

  inline std::string GetName() const {
    return name;
  }

  inline void SetName(std::string name) {
    this->name = name;
  }

  // Get rid of this, songs should not specify min note value
  // https://trello.com/c/WoH4c9LD
  uint32 GetNoteCount() const {
    return numMeasures * beatsPerMeasure * minNoteValue;
  }
  uint32 GetBeatsPerMeasure() const {
    return beatsPerMeasure;
  }
  // Get rid of this, songs should not specify min note value
  // https://trello.com/c/WoH4c9LD
  uint32 GetMinNoteValue() const {
    return minNoteValue;
  }
  uint32 GetTempo() const {
    return tempo;
  }
  void SetTempo(uint32 tempo) {
    this->tempo = tempo;
  }
  uint32 GetNumMeasures() const {
    return numMeasures;
  }

  void AddMeasures(uint32 numMeasures);
  const InstrumentInstance& AddInstrument(Instrument* newInstrument);
  void MoveInstrument(InstrumentInstance* instrumentInstance, int32 direction);
  void RemoveInstrument(InstrumentInstance* instrumentInstance);
  bool Save(std::string fileName);

  static Song* LoadSong(std::string fileName, std::function<Instrument*(std::string)> instrumentLoader);

  static void SetOnTrackAdded(std::function<void(Song::InstrumentInstance*, uint32 trackId, void* payload)> onTrackAddedFn, void* payload) {
    onTrackAdded = std::make_pair(payload, onTrackAddedFn);
  }
  static void SetOnTrackRemoved(std::function<void(Song::InstrumentInstance*, uint32 trackId, void* payload)> onTrackRemovedFn, void* payload) {
    onTrackRemoved = std::make_pair(payload, onTrackRemovedFn);
  }
};