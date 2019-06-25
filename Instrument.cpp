#include "Instrument.h"
#include "Logging.h"
#include "Sequencer.h" // TODO: Remove this dependency!
#include "SerializeImpl.h"

#include <stdexcept>
#include <fstream>

Track::Track() {

}

Track::Track(Track&& other) noexcept {
  this->colorScheme = other.colorScheme;
  other.colorScheme.clear();
  this->data = other.data;
  other.data.clear();
  this->decay = other.decay;
  other.decay = 0;
  this->lvol = other.lvol;
  other.lvol = 0;
  this->mute = other.mute;
  other.mute = 0;
  this->name = other.name;
  other.name.clear();
  this->rvol = other.rvol;
  other.rvol = 0;
  this->skip = other.skip;
  other.skip = 0;
  this->soundIndex = other.soundIndex;
  other.soundIndex = kInvalidSoundHandle;
}

Track::~Track() {
  Sequencer::Get().ReleaseSound(soundIndex);
}

void Track::AddNotes(uint32 noteCount, uint8 noteValue) {
  SDL_LockAudio();
  data.resize(data.size() + noteCount, noteValue);
  SDL_UnlockAudio();
}

void Track::SetNoteCount(uint32 noteCount, uint8 noteValue) {
  SDL_LockAudio();
  data.resize(noteCount, noteValue);
  SDL_UnlockAudio();
}


Instrument::Instrument(std::string instrumentName, uint32 numNotes) :
  name(instrumentName),
  numNotes(numNotes) {

}

Instrument::Instrument(const ReadSerializer& r, uint32 numNotes)
  : numNotes(numNotes) {
  if (!SerializeRead(r)) {
    std::string strError("Instrument: Unable to serialize (read)");
    MCLOG(Error, strError.c_str());
    throw std::runtime_error(strError);
  }
}

static constexpr const char* kVersionTag("version");
static constexpr const char* kVersionString("0.0.6");
static constexpr const char* kNameTag("name");
static constexpr const char* kTracksTag("tracks");
static constexpr const char* kColorSchemeTag("colorscheme");
static constexpr const char* kSoundsTag("sounds");
static constexpr const char* kClassTag("class");

bool Instrument::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  // Version
  if (!d.HasMember(kVersionTag) || !d[kVersionTag].IsString()) {
    MCLOG(Error, "Missing/invalid version tag in instrument file");
    return false;
  }
  std::string version = d[kVersionTag].GetString();

  // TODO: Check version

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
    const auto& trackEntry = tracksArray[trackArrayIndex];

    if (!trackEntry.HasMember(kNameTag) || !trackEntry[kNameTag].IsString()) {
      MCLOG(Error, "Invalid track in tracks array!");
      continue;
    }
    auto trackName = trackEntry[kNameTag].GetString();

    std::string colorScheme;
    if (trackEntry.HasMember(kColorSchemeTag) && trackEntry[kColorSchemeTag].IsString()) {
      colorScheme = trackEntry[kColorSchemeTag].GetString();
    }

    // Sounds
    if (!trackEntry.HasMember(kSoundsTag) || !trackEntry[kSoundsTag].IsArray()) {
      MCLOG(Error, "Invalid sounds array in track");
      return false;
    }

    const auto& soundsArray = trackEntry[kSoundsTag];

    if (soundsArray.Size()) {
      // Take the first one for now
      const auto& soundsEntry = soundsArray[0];

      // Get factory
      if (!soundsEntry.HasMember(kClassTag) || !soundsEntry[kClassTag].IsString()) {
        MCLOG(Error, "No class tag for sound");
        return false;
      }
      std::string className(soundsEntry[kClassTag].GetString());

      const auto& soundInfoMap = SoundFactory::GetInfoMap();
      const auto& soundInfo = soundInfoMap.find(className);
      if (soundInfo == soundInfoMap.end()) {
        MCLOG(Error, "Invalid class tag for sound");
        return false;
      }

      try {
        AddTrack(trackName, colorScheme,
          soundInfo->second.createSerialized({ soundsEntry }));
      }
      catch (...) {
      }
    }
    else {
      MCLOG(Warn, "Empty sounds array in track");
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
    auto& track = tracks[trackIndex];

    w.StartObject();

    // Name tag:string
    w.Key(kNameTag);
    w.String(track.name.c_str());

    // Color scheme tag:string
    if (track.colorScheme.length()) {
      w.Key(kColorSchemeTag);
      w.String(track.colorScheme.c_str());
    }

    // TODO: Eventually handle dynamics (piano, forte, etc.). For right now
    // we'll only have one sound per track.
    w.Key(kSoundsTag);
    w.StartArray();

    Sound* sound = Sequencer::Get().GetSound(track.soundIndex);
    if (sound != nullptr) {
      w.StartObject();

      // Class tag:string
      w.Key(kClassTag);
      w.String(sound->GetClassName());

      sound->SerializeWrite({ w });

      w.EndObject();
    }

    w.EndArray();

    w.EndObject();
  }

  w.EndArray();
  w.EndObject();

  return true;
}

Instrument::~Instrument() {
  Clear();
}

void Instrument::ClearNotes() {
  for (auto& track : tracks) {
    std::fill(track.data.begin(), track.data.end(), 0);
  }
}

void Instrument::Clear() {
  tracks.clear();
}

void Instrument::SetNoteCount(uint32 numNotes) {
  for (auto& track : tracks) {
    track.SetNoteCount(numNotes);
  }
}

void Instrument::AddTrack(std::string voiceName, std::string colorScheme, Sound* synthSound) {
  auto soundIndex = Sequencer::Get().AddSound(synthSound);
  if (soundIndex != -1) {
    auto trackIndex = tracks.size();
    tracks.resize(trackIndex + 1);

    tracks[trackIndex].name = voiceName;
    tracks[trackIndex].colorScheme = colorScheme;
    tracks[trackIndex].soundIndex = soundIndex;

    // TODO: Do we always want to do this?
    tracks[trackIndex].AddNotes(numNotes, 0);
  }
}

void Instrument::PlayTrack(uint32 trackIndex, float velocity) {
  SDL_LockAudio();

  // Set a minimum velocity
  velocity = 0.3f + velocity * 0.7f;

  const Track& track = tracks[trackIndex];

  // TODO: This makes no sense, Sequencer calls PlayTrack which calls Sequencer::Play?
  // TODO: Shouldn't rely only on lvol
  Sequencer::Get().PlaySound(track.soundIndex, velocity * track.lvol);

  SDL_UnlockAudio();
}

void Instrument::SetTrackNote(uint32 trackIndex, uint32 noteIndex, float noteVelocity) {
  if (trackIndex < tracks.size()) {

    SDL_LockAudio();
    if (noteIndex >= tracks[trackIndex].data.size()) {
      tracks[trackIndex].data.resize(noteIndex + 1, 0);
    }
    tracks[trackIndex].data[noteIndex] =
      static_cast<uint8>(noteVelocity * kNoteVelocityAsUint8);
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

  if (SerializeWrite({ w })) {
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

