#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include "Track.h"
#include <string>
#include <map>

class InstrumentInstance;
class Instrument {
public:
  static constexpr const char* kDefaultName = "My Instrument";
private:
  static std::function<Instrument*(std::string)> instrumentLoader;

  std::map<uint32, Track*> tracksById;
  std::string name;
  std::string fileName;
  uint32 nextTrackId = 0;
  std::vector<InstrumentInstance*> instrumentInstances;

  static std::map<std::string, Instrument*> instrumentsByPath;
  static uint32 nextUniqueTrackId;
  static uint32 nextUniqueNoteId;
  static uint32 nextUniqueInstrumentId;
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

  inline const std::map<uint32, Track*>& GetTracks() const {
    return tracksById;
  }

  static Instrument* LoadInstrumentFile(std::string fileName);
  static Instrument* LoadInstrumentName(std::string name);
  static void SetLoadCallback(const std::function<Instrument*(std::string)>& loadCallback);
  static void FlushInstruments();
  static uint32 NextUniqueTrackId();
  static uint32 NextUniqueNoteId();
  static uint32 NextUniqueInstrumentId();

  std::pair<bool, std::string> SerializeRead(const ReadSerializer& serializer);
  std::pair<bool, std::string> SerializeWrite(const WriteSerializer& serializer);

  void AddTrack(Track* track);
  bool SaveInstrument(std::string fileName);
  void ReplaceTrackById(uint32 trackId, Track* track);
  void RemoveTrackById(uint32 trackId);
  Track* GetTrackById(uint32 trackId);
  InstrumentInstance* Instance();
  void RemoveInstance(InstrumentInstance* instrumentInstance);
};

