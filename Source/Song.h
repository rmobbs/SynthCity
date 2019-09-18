#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include "Globals.h"
#include <string>
#include <vector>
#include <list>
#include <functional>

class Instrument;
class Song {
public:
  static constexpr uint32 kDefaultNumMeasures = 4;
  static constexpr uint32 kDefaultBeatsPerMeasure = 4;
  static constexpr const char* kDefaultName = "Untitled";

  class Note {
  protected:
    int32 beatIndex = -1;
    int32 gameIndex = -1;

  public:
    Note() = default;
    
    inline Note(uint32 beatIndex, int32 gameIndex)
      : beatIndex(beatIndex)
      , gameIndex(gameIndex) {
    }

    inline int32 GetBeatIndex() const {
      return beatIndex;
    }
    inline void SetBeatIndex(int32 beatIndex) {
      this->beatIndex = beatIndex;
    }

    inline int32 GetGameIndex() const {
      return gameIndex;
    }
    inline void SetGameIndex(int32 gameIndex) {
      this->gameIndex = gameIndex;
    }
  };
  
  struct LineIterator {
    std::list<Note>::iterator beg;
    std::list<Note>::iterator cur;
    std::list<Note>::iterator end;
  };

protected:
  uint32 tempo = Globals::kDefaultTempo;
  uint32 numMeasures = 0;
  uint32 beatsPerMeasure = kDefaultBeatsPerMeasure;
  uint32 minNoteValue = Globals::kDefaultMinNote;
  std::string name;

  // TODO: Permit songs to reference multiple instruments
  // https://trello.com/c/8iaMDKmY
  Instrument* instrument = nullptr;

  std::vector<std::list<Note>> barLines;
  std::function<Instrument*(std::string)> instrumentLoader;

  static Song* LoadSongMidi(std::string fileName, std::function<Instrument*(std::string)> instrumentLoader);
  static Song* LoadSongJson(std::string fileName, std::function<Instrument*(std::string)> instrumentLoader);

public:
  Song(std::string name, uint32 tempo, uint32 numLines, uint32 numMeasures, uint32 beatsPerMeasure, uint32 minNoteValue);
  Song(const ReadSerializer& serializer, std::function<Instrument*(std::string)> instrumentLoader);
  ~Song();

  std::pair<bool, std::string> SerializeRead(const ReadSerializer& serializer);
  std::pair<bool, std::string> SerializeWrite(const WriteSerializer& serializer);

  std::string GetInstrumentName() const;

  inline Instrument* GetInstrument() const {
    return instrument;
  }

  inline const std::vector<std::list<Note>>& GetBarLines() const {
    return barLines;
  }

  inline std::string GetName() const {
    return name;
  }

  inline void SetName(std::string name) {
    this->name = name;
  }

  uint32 GetNoteCount() const {
    return numMeasures * beatsPerMeasure * minNoteValue;
  }
  uint32 GetLineCount() const {
    return barLines.size();
  }
  std::list<Note>& GetLine(uint32 lineIndex) {
    return barLines[lineIndex];
  }
  uint32 GetBeatsPerMeasure() const {
    return beatsPerMeasure;
  }
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

  std::vector<LineIterator> GetIterator() {
    std::vector<LineIterator> iters;
    for (auto& line : barLines) {
      iters.push_back({ line.begin(), line.begin(), line.end() });
    }
    return iters;
  }

  void AddMeasures(uint32 numMeasures);
  void AddLine();
  void RemoveLine(uint32 lineIndex);
  Note* AddNote(uint32 lineIndex, uint32 beatIndex);
  void RemoveNote(uint32 lineIndex, uint32 beatIndex);
  void SetInstrument(Instrument* newInstrument);
  bool Save(std::string fileName);

  static Song* LoadSong(std::string fileName, std::function<Instrument*(std::string)> instrumentLoader);
};