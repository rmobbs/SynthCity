#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include "Track.h"
#include <string>
#include <vector>

class Sound;

class Instrument {
public:
  std::vector<Track*> tracks;
  std::string name;
  uint32 numNotes;
  int32 soloTrack = -1;

  void AddTrack(Track* track);
  void ReplaceTrack(uint32 index, Track* track);
  void RemoveTrack(uint32 index);
  void ClearNotes();
  void Clear();
  void PlayTrack(uint32 trackIndex);
  void SetNoteCount(uint32 numNotes);
  void SetTrackNote(uint32 trackIndex, uint32 noteIndex, bool onOrOff);
  bool SaveInstrument(std::string fileName);

  Instrument(const ReadSerializer& r, uint32 numNotes);
  Instrument(std::string instrumentName, uint32 numNotes);
  ~Instrument();

  std::string GetName(void) const {
    return name;
  }
  void SetName(const std::string& name);

  inline const std::vector<Track*>& GetTracks(void) const {
    return tracks;
  }

  inline int32 GetSoloTrack() const {
    return soloTrack;
  }

  void SetSoloTrack(int32 trackIndex);

  static Instrument* LoadInstrument(std::string fileName, uint32 numNotes);

  bool SerializeRead(const ReadSerializer& serializer);
  bool SerializeWrite(const WriteSerializer& serializer);
};

