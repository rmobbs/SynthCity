#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include "Globals.h"
#include <string>
#include <map>
#include <list>
#include <functional>

class Instrument;
class InstrumentInstance;

class Song {
public:
  static constexpr uint32 kDefaultNumMeasures = 4;
  static constexpr uint32 kDefaultBeatsPerMeasure = 4;
  static constexpr const char* kDefaultName = "Untitled";

protected:
  static Song* singleton;

  uint32 tempo = Globals::kDefaultTempo;
  uint32 numMeasures = 0;
  uint32 beatsPerMeasure = kDefaultBeatsPerMeasure;
  uint32 minNoteValue = Globals::kDefaultMinNote;
  std::string name;
  std::string fileName;

  static uint32 nextUniqueTrackId;
  static uint32 nextUniqueNoteId;
  static uint32 nextUniqueInstrumentInstanceId;

  std::list<InstrumentInstance*> instrumentInstances;

  static Song* LoadSongMidi(std::string fileName);
  static Song* LoadSongJson(std::string fileName);

  std::pair<bool, std::string> SerializeReadInstrument23(const ReadSerializer& serializer);

  // Force using the LoadSong/NewSong interface to set the singleton
  Song(std::string name, uint32 tempo, uint32 numMeasures, uint32 beatsPerMeasure, uint32 minNoteValue);
  Song(const ReadSerializer& serializer);

public:
  ~Song();

  std::pair<bool, std::string> SerializeRead(const ReadSerializer& serializer);
  std::pair<bool, std::string> SerializeWrite(const WriteSerializer& serializer);

  inline const std::list<InstrumentInstance*>& GetInstrumentInstances() const {
    return instrumentInstances;
  }

  inline std::string GetName() const {
    return name;
  }

  inline void SetName(std::string name) {
    this->name = name;
  }

  inline std::string GetFileName() const {
    return fileName;
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
  void SetTempo(uint32 tempo);

  uint32 GetNumMeasures() const {
    return numMeasures;
  }

  void AddMeasures(uint32 numMeasures);
  void AddInstrumentInstance(InstrumentInstance* instrumentInstance);
  void MoveInstrumentInstance(InstrumentInstance* instrumentInstance, int32 direction);
  void RemoveInstrumentInstance(InstrumentInstance* instrumentInstance);
  bool Save(std::string fileName);
  bool Save();

  static bool NewSong();
  static bool LoadSong(std::string fileName);
  static Song* Get() {
    return singleton;
  }
  static void Term();
};