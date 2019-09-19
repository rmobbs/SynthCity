#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include "Track.h"
#include <string>
#include <map>

class Instrument {
public:
  static constexpr const char* kDefaultName = "My Instrument";
private:
  std::map<uint32, Track*> tracksById;
  std::string name;
  std::string fileName;
  uint32 soloTrackId = kInvalidUint32;
  uint32 nextTrackId = 0;
public:
  Instrument(const ReadSerializer& r);
  Instrument(std::string instrumentName);
  ~Instrument();

  std::string GetName(void) const {
    return name;
  }
  void SetName(const std::string& name);

  inline std::string GetFileName() const {
    return fileName;
  }

  inline int32 GetSoloTrackId() const {
    return soloTrackId;
  }
  void SetSoloTrackById(uint32 trackId);

  inline const std::map<uint32, Track*>& GetTracks() const {
    return tracksById;
  }

  static Instrument* LoadInstrument(std::string fileName);

  std::pair<bool, std::string> SerializeRead(const ReadSerializer& serializer);
  std::pair<bool, std::string> SerializeWrite(const WriteSerializer& serializer);

  void AddTrack(Track* track);
  bool SaveInstrument(std::string fileName);
  void ReplaceTrackById(uint32 trackId, Track* track);
  void RemoveTrackById(uint32 trackId);
  Track* GetTrackById(uint32 trackId);
};

