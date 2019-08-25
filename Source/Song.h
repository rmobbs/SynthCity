#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include <string>
#include <vector>

class Song {
public:
  class Note {
  protected:
    bool enabled = false;
    int32 gameIndex = -1;

  public:
    Note() = default;
    
    inline Note(bool enabled, int32 gameIndex)
      : enabled(enabled)
      , gameIndex(gameIndex) {
    }

    inline void SetEnabled(bool enabled) {
      this->enabled = enabled;
    }
    inline bool GetEnabled() const {
      return enabled;
    }

    inline int32 GetGameIndex() const {
      return gameIndex;
    }
    inline void SetGameIndex(int32 gameIndex) {
      this->gameIndex = gameIndex;
    }
  };

protected:
  uint32 tempo = 0;
  uint32 beatsPerMeasure = 0;
  uint32 noteValue = 4;
  uint32 beatSubdivision = 0;
  std::string instrumentName;

  std::vector<std::vector<Note>> barLines;

public:
  Song(uint32 numLines, uint32 tempo, uint32 numMeasures, uint32 beatsPerMeasure, uint32 beatSubdivision);
  Song(const ReadSerializer& serializer);

  std::pair<bool, std::string> SerializeRead(const ReadSerializer& serializer);

  inline const std::string& GetInstrumentName() const {
    return instrumentName;
  }

  inline const std::vector<std::vector<Note>>& GetBarLines() const {
    return barLines;
  }

  uint32 GetNoteCount() const {
    if (barLines.size()) {
      return barLines[0].size();
    }
    return 0;
  }
  uint32 GetLineCount() const {
    return barLines.size();
  }
  std::vector<Note>& GetLine(uint32 lineIndex) {
    return barLines[lineIndex];
  }
  uint32 GetBeatsPerMeasure() const {
    return beatsPerMeasure;
  }
  uint32 GetBeatSubdivision() const {
    return beatSubdivision;
  }
  uint32 GetTempo() const {
    return tempo;
  }
  void SetTempo(uint32 tempo) {
    this->tempo = tempo;
  }
  uint32 GetNumMeasures() const {
    return GetNoteCount() / (beatSubdivision * beatsPerMeasure);
  }

  void AddLine();
  void RemoveLine(uint32 lineIndex);
  void ToggleNoteEnabled(uint32 lineIndex, uint32 noteIndex);
  void SetNoteGameIndex(uint32 lineIndex, uint32 noteIndex, int32 gameIndex);
};