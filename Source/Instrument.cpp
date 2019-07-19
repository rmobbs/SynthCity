#include "Instrument.h"
#include "Logging.h"
#include "Sequencer.h" // TODO: Remove this dependency!
#include "SerializeImpl.h"
#include "SoundFactory.h"
#include "WavSound.h"
#include "Globals.h"

#include <stdexcept>
#include <fstream>

static constexpr const char* kVersionTag("version");
static constexpr const char* kNameTag("name");
static constexpr const char* kTracksTag("tracks");
static constexpr const char* kColorSchemeTag("colorscheme");
static constexpr const char* kSoundsTag("sounds");

static constexpr const char* kCurrentVersion("0.0.7");

Instrument::Instrument(std::string instrumentName, uint32 numNotes) :
  name(instrumentName),
  numNotes(numNotes) {

}

Instrument::Instrument(const ReadSerializer& r, uint32 numNotes)
  : numNotes(numNotes) {
  if (!SerializeRead(r)) {
    throw std::runtime_error("Instrument: Unable to serialize (read)");
  }
}

Instrument::~Instrument() {
  Clear();
}

bool Instrument::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  // Version
  if (!d.HasMember(kVersionTag) || !d[kVersionTag].IsString()) {
    MCLOG(Error, "Missing/invalid version tag in instrument file");
    return false;
  }
  std::string version = d[kVersionTag].GetString();

  if (version != std::string(kCurrentVersion)) {
    MCLOG(Error, "Invalid instrument file version");
    return false;
  }

  // Name
  if (!d.HasMember(kNameTag) || !d[kNameTag].IsString()) {
    MCLOG(Error, "Missing/invalid name tag in instrument file");
    return false;
  }
  SetName(d[kNameTag].GetString());

  // Tracks
  if (!d.HasMember(kTracksTag) || !d[kTracksTag].IsArray()) {
    MCLOG(Error, "Invalid tracks array in instrument file");
    return false;
  }

  const auto& tracksArray = d[kTracksTag];
  for (rapidjson::SizeType trackArrayIndex = 0; trackArrayIndex < tracksArray.Size(); ++trackArrayIndex) {
    try {
      AddTrack(new Track({ tracksArray[trackArrayIndex] }));
    }
    catch (...) {

    }
  }

  return true;
}

bool Instrument::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.StartObject();

  // Version tag:string
  w.Key(kVersionTag);
  w.String(kVersionString);

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

void Instrument::ClearNotes() {
  for (auto& track : tracks) {
    track->ClearNotes();
  }
}

void Instrument::Clear() {
  for (auto& track : tracks) {
    delete track;
  }
  tracks.clear();
}

void Instrument::SetNoteCount(uint32 numNotes) {
  for (auto& track : tracks) {
    track->SetNoteCount(numNotes);
  }
}

void Instrument::AddTrack(Track* track) {
  track->SetNoteCount(numNotes);
  tracks.push_back(track);
}

void Instrument::PlayTrack(uint32 trackIndex, float velocity) {
  // Ensure a minimum velocity
  velocity = 0.3f + velocity * 0.7f;

  Mixer::Get().PlayPatch(tracks[trackIndex]->GetPatch(), velocity);
}

void Instrument::SetTrackNote(uint32 trackIndex, uint32 noteIndex, float noteVelocity) {
  if (trackIndex < tracks.size()) {

    SDL_LockAudio();
    tracks[trackIndex]->SetNote(noteIndex,
      static_cast<uint8>(noteVelocity * kNoteVelocityAsUint8));
    SDL_UnlockAudio();
  }
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
Instrument* Instrument::LoadInstrument(std::string fileName, uint32 numNotes) {
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
    newInstrument = new Instrument({ document }, numNotes);
  }
  catch (...) {

  }
  return newInstrument;
}

void Instrument::SetName(const std::string& name) {
  this->name = name;
}

