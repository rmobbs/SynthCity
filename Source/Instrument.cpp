#include "Instrument.h"
#include "Logging.h"
#include "Sequencer.h" // TODO: Remove this dependency!
#include "SerializeImpl.h"
#include "SoundFactory.h"
#include "WavSound.h"
#include "Globals.h"
#include "AudioGlobals.h"

#include <stdexcept>
#include <fstream>

static constexpr const char* kNameTag("name");
static constexpr const char* kTracksTag("tracks");
static constexpr const char* kColorSchemeTag("colorscheme");
static constexpr const char* kSoundsTag("sounds");
static constexpr uint32 kInstrumentFileVersion = 1;

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
  for (auto& track : tracks) {
    delete track;
  }
  tracks.clear();
}

std::pair<bool, std::string> Instrument::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  // Version
  if (!d.HasMember(Globals::kVersionTag) || !d[Globals::kVersionTag].IsUint()) {
    return std::make_pair(false, "Missing/invalid version tag in instrument file");
  }

  auto version = d[Globals::kVersionTag].GetUint();

  if (version != kInstrumentFileVersion) {
    // Allow conversion of previous formats
    // https://trello.com/c/O0SzcHfG
    return std::make_pair(false, "Invalid instrument file version");
  }


  // Name
  if (!d.HasMember(kNameTag) || !d[kNameTag].IsString()) {
    return std::make_pair(false, "Missing/invalid name tag in instrument file");
  }
  SetName(d[kNameTag].GetString());

  // Tracks
  if (!d.HasMember(kTracksTag) || !d[kTracksTag].IsArray()) {
    return std::make_pair(false, "Invalid tracks array in instrument file");
  }

  const auto& tracksArray = d[kTracksTag];
  for (rapidjson::SizeType trackArrayIndex = 0; trackArrayIndex < tracksArray.Size(); ++trackArrayIndex) {
    try {
      AddTrack(new Track({ tracksArray[trackArrayIndex] }));
    }
    catch (...) {

    }
  }

  return std::make_pair(true, "");
}

bool Instrument::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.StartObject();

  // Version tag:string
  w.Key(Globals::kVersionTag);
  w.Uint(kInstrumentFileVersion);

  // Name tag:string
  w.Key(kNameTag);
  w.String(name.c_str());

  // Tracks tag:array_start
  w.Key(kTracksTag);
  w.StartArray();

  // Tracks
  for (uint32 trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
    tracks[trackIndex]->SerializeWrite(serializer);
  }

  w.EndArray();
  w.EndObject();

  return true;
}

void Instrument::AddTrack(Track* track) {
  tracks.push_back(track);
}

void Instrument::ReplaceTrack(uint32 index, Track* newTrack) {
  delete tracks[index];
  tracks[index] = newTrack;
}

void Instrument::RemoveTrack(uint32 index) {
  if (soloTrack == index) {
    soloTrack = -1;
  }
  delete tracks[index];
  tracks.erase(tracks.begin() + index);
}

void Instrument::SetSoloTrack(int32 trackIndex) {
  soloTrack = trackIndex;
  if (trackIndex != -1) {
    tracks[trackIndex]->SetMute(false);
  }
}

void Instrument::PlayTrack(uint32 trackIndex) {
  Mixer::Get().PlayPatch(tracks[trackIndex]->GetPatch(), tracks[trackIndex]->GetVolume());
}

Track* Instrument::GetTrack(uint32 trackIndex) {
  if (trackIndex < tracks.size()) {
    return tracks[trackIndex];
  }
  return nullptr;
}

bool Instrument::SaveInstrument(std::string fileName) {
  std::ofstream ofs(fileName);
  if (ofs.bad()) {
    MCLOG(Warn, "Unable to save instrument to file %s ", fileName.c_str());
    return false;
  }

  rapidjson::StringBuffer sb;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);

  if (SerializeWrite({ w, std::filesystem::path(fileName).parent_path() })) {
    std::string outputString(sb.GetString());
    ofs.write(outputString.c_str(), outputString.length());
    ofs.close();
    return true;
  }
  return false;
}

/* static */
Instrument* Instrument::LoadInstrument(std::string fileName) {
  MCLOG(Info, "Loading instrument from file \'%s\'", fileName.c_str());

  std::ifstream ifs(fileName);

  if (ifs.bad()) {
    MCLOG(Warn, "Unable to load instrument from file %s", fileName.c_str());
    return nullptr;
  }

  std::string fileData((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  if (!fileData.length()) {
    MCLOG(Warn, "Unable to load instrument from file %s", fileName.c_str());
    return nullptr;
  }

  // Create JSON parser
  rapidjson::Document document;
  document.Parse(fileData.c_str());

  if (!document.IsObject()) {
    MCLOG(Warn, "Failure parsing JSON in file %s", fileName.c_str());
    return nullptr;
  }

  Instrument* newInstrument = nullptr;
  try {
    newInstrument = new Instrument({ document });
  }
  catch (std::runtime_error& rte) {
    MCLOG(Error, "Failed to serialize instrument: %s", rte.what());
  }
  return newInstrument;
}

void Instrument::SetName(const std::string& name) {
  this->name = name;
}

