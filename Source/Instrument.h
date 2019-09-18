#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include "Track.h"
#include <string>
#include <vector>

class Instrument {
public:
  static constexpr const char* kDefaultName = "My Instrument";

  std::vector<Track*> tracks;
  std::string name;
  int32 soloTrack = -1;

  void AddTrack(Track* track);
  void ReplaceTrack(uint32 index, Track* track);
  void RemoveTrack(uint32 index);
  void PlayTrack(uint32 trackIndex);
  bool SaveInstrument(std::string fileName);

  Instrument(const ReadSerializer& r);
  Instrument(std::string instrumentName);
  ~Instrument();

  std::string GetName(void) const {
    return name;
  }
  void SetName(const std::string& name);

  inline const std::vector<Track*>& GetTracks(void) const {
    return tracks;
  }

  inline uint32 GetTrackCount() const {
    return tracks.size();
  }

  inline int32 GetSoloTrack() const {
    return soloTrack;
  }
  Track* GetTrack(uint32 trackIndex);

  void SetSoloTrack(int32 trackIndex);

  static Instrument* LoadInstrument(std::string fileName);

  std::pair<bool, std::string> SerializeRead(const ReadSerializer& serializer);
  bool SerializeWrite(const WriteSerializer& serializer);
};

