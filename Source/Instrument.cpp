#include "Instrument.h"
#include "InstrumentInstance.h"
#include "Logging.h"
#include "SerializeImpl.h"
#include "SoundFactory.h"
#include "WavSound.h"
#include "Globals.h"
#include "AudioGlobals.h"
#include "OddsAndEnds.h"

#include <stdexcept>
#include <fstream>

static constexpr const char* kTracksTag("tracks");
static constexpr const char* kColorSchemeTag("colorscheme");
static constexpr const char* kSoundsTag("sounds");
static constexpr uint32 kInstrumentFileVersion = 2;
static constexpr const char* kNextUniqueIdTag("nextid");

Instrument::Instrument() {

}

Instrument::Instrument(std::string instrumentName)
  : name(instrumentName) {

}

Instrument::Instrument(const ReadSerializer& r) {
  auto result = SerializeRead(r);
  if (!result.first) {
    throw std::runtime_error(result.second);
  }
}

Instrument::~Instrument() {
  for (const auto& track : tracksById) {
    delete track.second;
  }
  tracksById.clear();

  for (const auto& instance : instrumentInstances) {
    delete instance;
  }
  instrumentInstances.clear();
}

void Instrument::SetName(const std::string& name) {
  this->name = name;
}

Track* Instrument::GetTrackById(uint32 trackId) {
  auto trackById = tracksById.find(trackId);
  if (trackById != tracksById.end()) {
    return trackById->second;
  }
  return nullptr;
}

void Instrument::AddTrack(Track* track) {
  auto trackId = track->GetUniqueId();
  if (trackId == kInvalidUint32) {
    trackId = nextTrackId++;
  }
  assert(tracksById.find(trackId) == tracksById.end());
  tracksById[trackId] = track;
  track->SetUniqueId(trackId);

  for (auto& instance : instrumentInstances) {
    instance->trackInstances.insert({ trackId, { trackId } });
  }
}

void Instrument::ReplaceTrackById(uint32 trackId, Track* newTrack) {
  auto trackEntry = tracksById.find(trackId);
  assert(trackEntry != tracksById.end());
  if (trackEntry != tracksById.end()) {
    delete trackEntry->second;

    // TODO: see if modifying the iterator works
    tracksById[trackId] = newTrack;
    newTrack->SetUniqueId(trackId);
  }
}

void Instrument::RemoveTrackById(uint32 trackId) {
  auto trackEntry = tracksById.find(trackId);
  assert(trackEntry != tracksById.end());
  if (trackEntry != tracksById.end()) {
    delete trackEntry->second;
    tracksById.erase(trackEntry);

    for (auto& instance : instrumentInstances) {
      auto trackInstance = instance->trackInstances.find(trackId);
      if (trackInstance != instance->trackInstances.end()) {
        instance->trackInstances.erase(trackInstance);
      }
    }
  }
}

InstrumentInstance* Instrument::Instance() {
  instrumentInstances.push_back(new InstrumentInstance(this));
  return instrumentInstances.back();
}

void Instrument::RemoveInstance(InstrumentInstance* instrumentInstance) {
  auto instrumentInstanceIter = std::find(instrumentInstances.begin(), instrumentInstances.end(), instrumentInstance);
  assert(instrumentInstanceIter != instrumentInstances.end());
  instrumentInstances.erase(instrumentInstanceIter);
  delete instrumentInstance;
}

std::pair<bool, std::string> Instrument::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  fileName = serializer.fileName;

  // Version
  if (!d.HasMember(Globals::kVersionTag) || !d[Globals::kVersionTag].IsUint()) {
    return std::make_pair(false, "Missing/invalid version tag in instrument file");
  }

  auto version = d[Globals::kVersionTag].GetUint();

  if (version != 1 && version != 2) {
    return std::make_pair(false, "Invalid instrument file version");
  }

  // Name
  if (!d.HasMember(Globals::kNameTag) || !d[Globals::kNameTag].IsString()) {
    return std::make_pair(false, "Missing/invalid name tag in instrument file");
  }
  SetName(d[Globals::kNameTag].GetString());

  if (version > 1) {
    // Next unique ID
    if (!d.HasMember(kNextUniqueIdTag) || !d[kNextUniqueIdTag].IsUint()) {
      return std::make_pair(false, "Missing/invalid next unique ID tag in instrument file");
    }
    nextTrackId = d[kNextUniqueIdTag].GetUint();
  }

  // Tracks
  if (!d.HasMember(kTracksTag) || !d[kTracksTag].IsArray()) {
    return std::make_pair(false, "Invalid tracks array in instrument file");
  }

  // @DEPRECATE For version 1 songs, will be deprecated in version 3
  uint32 loadIndex = 0;

  const auto& tracksArray = d[kTracksTag];
  for (rapidjson::SizeType trackArrayIndex = 0; trackArrayIndex < tracksArray.Size(); ++trackArrayIndex) {
    try {
      auto track = new Track({ tracksArray[trackArrayIndex] });
      track->SetLoadIndex(loadIndex++);
      AddTrack(track);
    }
    catch (...) {

    }
  }

  if (version < 2) {
    MCLOG(Warn, "Instrument is version 1 and this will be deprecated. Please re-save.")
    nextTrackId = tracksById.size();
  }

  return std::make_pair<bool, std::string>(true, {});
}

std::pair<bool, std::string> Instrument::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.StartObject();

  // Version tag:string
  w.Key(Globals::kVersionTag);
  w.Uint(kInstrumentFileVersion);

  // Name tag:string
  w.Key(Globals::kNameTag);
  w.String(name.c_str());

  // Next unique ID
  w.Key(kNextUniqueIdTag);
  w.Uint(nextTrackId);

  // Tracks tag:array_start
  w.Key(kTracksTag);
  w.StartArray();

  // Tracks
  for (const auto& track : tracksById) {
    track.second->SerializeWrite(serializer);
  }

  w.EndArray();
  w.EndObject();

  return std::make_pair<bool, std::string>(true, {});
}

bool Instrument::SaveInstrument(std::string fileName) {
  ensure_fileext(fileName, Globals::kJsonTag);

  std::ofstream ofs(fileName);
  if (ofs.bad()) {
    MCLOG(Error, "Unable to save instrument to file %s ", fileName.c_str());
    return false;
  }

  rapidjson::StringBuffer sb;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);

  auto result = SerializeWrite({ w, std::filesystem::path(fileName).parent_path() });
  if (result.first) {
    std::string outputString(sb.GetString());
    ofs.write(outputString.c_str(), outputString.length());
    ofs.close();
    return true;
  }
  MCLOG(Error, result.second.c_str());
  return false;
}
