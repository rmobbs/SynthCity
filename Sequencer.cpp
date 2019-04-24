#include "Sequencer.h"
#include "SDL.h"
#include <iostream>
#include "mixer.h"
#include "SDL_audio.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include "tinyxml2.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "MidiSource.h"
#include "logging.h"

static constexpr int kAudioBufferSize = 2048;
static constexpr float kMetronomeVolume = 0.7f;
static constexpr uint32 kNoteVelocityAsUint8 = 255;
static constexpr const char *kInstrumentTag = "Instrument";
static constexpr const char *kTempoTag = "Tempo";
static constexpr const char *kMeasuresTag = "Measures";
static constexpr const char *kBeatsPerMeasureTag = "BeatsPerMeasure";
static constexpr const char *kNotesTag = "Notes";
static constexpr const char *kBeatTag = "Beat";
static constexpr const char *kTrackTag = "Track";
static constexpr const char *kVelocityTag = "Velocity";
static constexpr std::string_view kMidiTags[] = { ".midi", ".mid" };
static constexpr std::string_view kJsonTag(".json");


enum class Voices {
  Reserved1,
  ReservedCount,
};

enum class Sounds {
  MetronomeFull,
  MetronomePartial,
  Count,
};

///////////////////////////////////////////////////////////////////////////////
// Sequencer::Track
///////////////////////////////////////////////////////////////////////////////
Sequencer::Track::Track() {

}

Sequencer::Track::Track(Track&& other) noexcept {
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
  other.soundIndex = Mixer::kInvalidSoundHandle;
  this->voiceIndex = other.voiceIndex;
  other.voiceIndex = Mixer::kInvalidVoiceHandle;
}

Sequencer::Track::~Track() {
  Mixer::Get().ReleaseVoice(voiceIndex);
  Mixer::Get().ReleaseSound(soundIndex);
}

void Sequencer::Track::AddNotes(uint32 noteCount, uint8 noteValue) {
  SDL_LockAudio();
  data.resize(data.size() + noteCount, noteValue);
  SDL_UnlockAudio();
}

void Sequencer::Track::SetNoteCount(uint32 noteCount, uint8 noteValue) {
  SDL_LockAudio();
  data.resize(noteCount, noteValue);
  SDL_UnlockAudio();
}

///////////////////////////////////////////////////////////////////////////////
// Sequencer::Instrument
///////////////////////////////////////////////////////////////////////////////
Sequencer::Instrument::Instrument(std::string instrumentName, uint32 numNotes) :
  name(instrumentName),
  numNotes(numNotes) {

}

Sequencer::Instrument::~Instrument() {
  Clear();
}

void Sequencer::Instrument::ClearNotes() {
  for (auto& track : tracks) {
    std::fill(track.data.begin(), track.data.end(), 0);
  }
}

void Sequencer::Instrument::Clear() {
  tracks.clear();
}

void Sequencer::Instrument::SetNoteCount(uint32 numNotes) {
  for (auto& track : tracks) {
    track.SetNoteCount(numNotes);
  }
}

void Sequencer::Instrument::AddTrack(std::string voiceName, std::string colorScheme, std::string fileName) {
  auto soundIndex = Mixer::Get().LoadSound(fileName);
  if (soundIndex != -1) {
    auto trackIndex = tracks.size();
    tracks.resize(trackIndex + 1);

    auto voiceIndex = Mixer::Get().AddVoice();

    tracks[trackIndex].name = voiceName;
    tracks[trackIndex].colorScheme = colorScheme;
    tracks[trackIndex].soundIndex = soundIndex;
    tracks[trackIndex].voiceIndex = voiceIndex;

    // TODO: Do we always want to do this?
    tracks[trackIndex].AddNotes(numNotes, 0);
  }
}

void Sequencer::Instrument::PlayTrack(uint32 trackIndex, uint8 velocity) {
  SDL_LockAudio();
  float vel = velocity / 255.0f;
  if (vel) {
    vel = 0.3f + vel * 0.7f;
  }
  const Track& track = tracks[trackIndex];
  Mixer::Get().Play(track.voiceIndex, track.soundIndex, vel * track.lvol);
  SDL_UnlockAudio();
}

/* static */
Sequencer::Instrument *Sequencer::Instrument::LoadInstrument(std::string fileName, uint32 numNotes) {
  tinyxml2::XMLDocument doc;
  auto parseError = doc.LoadFile(fileName.c_str());
  if (parseError != tinyxml2::XML_SUCCESS) {
    return nullptr;
  }

  // <SynthCityInstrument ...>
  auto instrumentNode = doc.FirstChildElement("SynthCityInstrument");
  if (!instrumentNode) {
    // log error
    return nullptr;
  }
  const char* instrumentName = nullptr;
  parseError = instrumentNode->QueryStringAttribute("Name", &instrumentName);

  int version = 0;
  parseError = instrumentNode->QueryIntAttribute("Version", &version);
  if (parseError != tinyxml2::XML_SUCCESS) {
    // log error
    return nullptr;
  }

  // TODO: Check version

  // <Voices>
  auto voicesNode = instrumentNode->FirstChildElement("Voices");
  if (!voicesNode) {
    // log error
    return nullptr;
  }

  Instrument* newInstrument = new Instrument(instrumentName, numNotes);
  for (auto voiceNode = voicesNode->FirstChildElement("Voice"); voiceNode; voiceNode = voiceNode->NextSiblingElement("Voice")) {
    const char *voiceName = nullptr;
    voiceNode->QueryStringAttribute("Name", &voiceName);
    if (!voiceName || !strlen(voiceName)) {
      // log error
      continue;
    }

    const char *colorScheme = nullptr;
    voiceNode->QueryStringAttribute("ColorScheme", &colorScheme);

    auto wavsNode = voiceNode->FirstChildElement("Wavs");
    if (!wavsNode) {
      // TODO: synth
      continue;
    }

    std::vector<const char*> wavFiles;
    for (auto wavNode = wavsNode->FirstChildElement("Wav"); wavNode; wavNode = wavNode->NextSiblingElement("Wav")) {
      // TODO: Wav dynamic
      const char* fileName = nullptr;
      wavNode->QueryStringAttribute("File", &fileName);
      if (!fileName || !strlen(fileName)) {
        continue;
      }
      wavFiles.push_back(fileName);
    }

    if (!wavFiles.size()) {
      // log error
      continue;
    }

    newInstrument->AddTrack(voiceName, colorScheme, wavFiles[0]);
  }

  return newInstrument;
}

///////////////////////////////////////////////////////////////////////////////
// Sequencer
///////////////////////////////////////////////////////////////////////////////
void Sequencer::SetTrackNote(uint32 trackIndex, uint32 noteIndex, float noteVelocity) {
  if (instrument != nullptr) {
    if (trackIndex < instrument->tracks.size()) {

      SDL_LockAudio();
      if (noteIndex >= instrument->tracks[trackIndex].data.size()) {
        instrument->tracks[trackIndex].data.resize(noteIndex + 1, 0);
      }
      instrument->tracks[trackIndex].data[noteIndex] = 
        static_cast<uint8>(noteVelocity * kNoteVelocityAsUint8);
      SDL_UnlockAudio();
    }
  }
}

uint32 Sequencer::CalcInterval(uint32 beatSubdivision) const {
  if (currentBpm > 0 && beatSubdivision > 0) {
    return static_cast<uint32>(44100.0 / currentBpm * 60.0 / static_cast<float>(beatSubdivision));
  }
  return 0;
}

void Sequencer::SetSubdivision(uint32 subdivision) {
  if (subdivision > maxBeatSubdivisions) {
    subdivision = maxBeatSubdivisions;
  }
  currBeatSubdivision = subdivision;
  interval = static_cast<int>(CalcInterval(GetSubdivision()));
}

void Sequencer::SetBeatsPerMinute(uint32 bpm) {
  currentBpm = bpm;
  interval = static_cast<int>(CalcInterval(GetSubdivision()));
  Mixer::Get().ApplyInterval(interval);
}

void Sequencer::PartialNoteCallback() {
  // Nothin'
}

void Sequencer::FullNoteCallback(bool isMeasure) {
  if (IsMetronomeOn()) {
    uint32 metronomeSound = static_cast<uint32>(Sounds::MetronomePartial);
    if (isMeasure) {
      metronomeSound = static_cast<uint32>(Sounds::MetronomeFull);
    }
    Mixer::Get().Play(static_cast<uint32>(Voices::Reserved1), reservedSounds[metronomeSound], kMetronomeVolume);
  }
}

void Sequencer::Play() {
  isPlaying = true;
}

void Sequencer::Pause() {
  isPlaying = false;
}

void Sequencer::Stop() {
  isPlaying = false;
  SetPosition(0);
}

void Sequencer::SetPosition(uint32 newPosition) {
  currPosition = newPosition;
  nextPosition = currPosition;
}

void Sequencer::SetNumMeasures(uint32 numMeasures) {
  if (this->numMeasures != numMeasures) {
    this->numMeasures = numMeasures;

    if (instrument != nullptr) {
      instrument->SetNoteCount(GetNumMeasures() * GetBeatsPerMeasure() * GetMaxSubdivisions());
    }
  }
}

void Sequencer::SetBeatsPerMeasure(uint32 beatsPerMeasure) {
  if (this->beatsPerMeasure != beatsPerMeasure) {
    this->beatsPerMeasure = beatsPerMeasure;

    if (instrument != nullptr) {
      instrument->SetNoteCount(GetNumMeasures() * GetBeatsPerMeasure() * GetMaxSubdivisions());
    }
  }
}

void Sequencer::SetLooping(bool looping) {
  SDL_LockAudio();
  isLooping = looping;
  SDL_UnlockAudio();
}

uint32 Sequencer::GetPosition(void) const {
  return currPosition;
}

uint32 Sequencer::GetNextPosition(void) const {
  return nextPosition;
}

uint32 Sequencer::NextFrame(void)
{
  // NOTE: Called from SDL audio callback so SM_LockAudio is in effect

  // If nothing to do, wait current subdivision (TODO: See if we can wait _minimal_ subdivision ...
  // need to fix how we're triggering metronome for that)
  if (!isPlaying || !interval || !instrument || !instrument->tracks.size()) {
    return CalcInterval(GetSubdivision());
  }

  int32 noteCount = GetNumMeasures() * GetBeatsPerMeasure() * GetMaxSubdivisions();

  currPosition = nextPosition;
  nextPosition = currPosition + maxBeatSubdivisions / currBeatSubdivision;

  // Handle end-of-track / looping
  if (currPosition >= noteCount) {
    if (isLooping) {
      currPosition = 0;
      nextPosition = currPosition + maxBeatSubdivisions / currBeatSubdivision;
    }
    else {
      Stop();
      return CalcInterval(GetSubdivision());
    }
  }

  if ((currPosition % maxBeatSubdivisions) == 0) {
    FullNoteCallback((currPosition % (maxBeatSubdivisions * GetBeatsPerMeasure())) == 0);
  }
  else {
    PartialNoteCallback();
  }

  for (size_t trackIndex = 0; trackIndex < instrument->tracks.size(); ++trackIndex) {
    if (currPosition >= static_cast<int>(instrument->tracks[trackIndex].data.size())) {
      continue;
    }

    uint8* d = instrument->tracks[trackIndex].data.data() + currPosition;
    if (*d > 0) {
      if (notePlayedCallback != nullptr) {
        notePlayedCallback(trackIndex, currPosition, notePlayedPayload);
      }
      instrument->PlayTrack(trackIndex, *d);
    }
  }

  return interval;
}

void Sequencer::PlayInstrumentTrack(uint32 instrumentTrack, float noteVelocity) {
  if (instrument != nullptr) {
    instrument->PlayTrack(instrumentTrack, static_cast<uint8>(noteVelocity * kNoteVelocityAsUint8));
  }
}

void Sequencer::SetNotePlayedCallback(Sequencer::NotePlayedCallback notePlayedCallback, void* notePlayedPayload) {
  this->notePlayedCallback = notePlayedCallback;
  this->notePlayedPayload = notePlayedPayload;
}

void Sequencer::Clear() {
  SDL_LockAudio();
  if (instrument) {
    instrument->Clear();
  }
  SDL_UnlockAudio();
}

bool Sequencer::LoadInstrument(std::string fileName, std::string mustMatch) {
  Instrument *newInstrument = Sequencer::Instrument::
    LoadInstrument(fileName, GetMaxSubdivisions() * GetNumMeasures() * GetBeatsPerMeasure());

  if (newInstrument) {
    if (mustMatch.length() != 0 && mustMatch != newInstrument->GetName()) {
      delete newInstrument;
    }
    else {
      SDL_LockAudio();
      Stop();
      delete instrument;
      instrument = newInstrument;
      SDL_UnlockAudio();
      return true;
    }
  }
  return false;
}

bool Sequencer::SaveSong(std::string fileName) {
  if (!instrument) {
    MCLOG(Warn, "Somehow there is no instrument in SaveSong");
    return false;
  }

  std::ofstream ofs(fileName);
  if (ofs.bad()) {
    MCLOG(Warn, "Unable to save song to file %s ", fileName.c_str());
    return false;
  }

  rapidjson::StringBuffer sb;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);

  w.StartObject();
  w.Key(kInstrumentTag);
  w.String(instrument->GetName().c_str());
  w.Key(kTempoTag);
  w.Uint(GetBeatsPerMinute());
  w.Key(kMeasuresTag);
  w.Uint(GetNumMeasures());
  w.Key(kBeatsPerMeasureTag);
  w.Uint(GetBeatsPerMeasure());
  
  w.Key(kNotesTag);
  w.StartArray();

  // Write notes
  uint32 wholeBeatIndex = 0;
  uint32 subdivisionIndex = 0;
  for (size_t m = 0; m < GetNumMeasures(); ++m) {
    for (size_t b = 0; b < GetBeatsPerMeasure(); ++b) {
      for (size_t s = 0; s < GetMaxSubdivisions(); ++s) {
        for (uint32 trackIndex = 0; trackIndex < instrument->GetTracks().size(); ++trackIndex) {
          auto& track = instrument->GetTracks()[trackIndex];
          auto n = track.GetNotes()[subdivisionIndex];
          if (n != 0) {
            float floatBeat = static_cast<float>(wholeBeatIndex) + 1.0f +
              (static_cast<float>(s) / static_cast<float>(GetMaxSubdivisions()));
            w.StartObject();
            w.Key(kBeatTag);
            w.Double(floatBeat);
            w.Key(kTrackTag);
            w.Uint(trackIndex);
            w.Key(kVelocityTag);
            w.Double(static_cast<float>(n) / kNoteVelocityAsUint8);
            w.EndObject();
          }
        }
        ++subdivisionIndex;
      }
      ++wholeBeatIndex;
    }
  }

  w.EndArray();
  
  w.EndObject();

  std::string outputString(sb.GetString());
  ofs.write(outputString.c_str(), outputString.length());
  ofs.close();

  return true;
}

void Sequencer::LoadMidi(std::string fileName) {
  /*
  if (!instrument) {
    std::cerr << "Cannot load MIDI without instrument" << std::endl;
    return;
  }
  */

  MCLOG(Info, "Importing MIDI from file \'%s\'", fileName.c_str());

  MidiSource midiSource;
  if (!midiSource.openFile(fileName)) {
    MCLOG(Warn, "Failed to load MIDI file \'%s\'", fileName.c_str());
    return;
  }

  if (!midiSource.getTrackCount()) {
    MCLOG(Warn, "Loading MIDI file \'%s\' resulted in no tracks", fileName.c_str());
    return;
  }

  if (midiSource.getTrackCount() > 1) {
    // Have to pick a single track or merge a range of tracks

  }
}

void Sequencer::LoadJson(std::string fileName, std::function<bool(std::string)> loadInstrumentCallback) {
  MCLOG(Info, "Loading song from file \'%s\'", fileName.c_str());

  std::ifstream ifs(fileName);

  if (ifs.bad()) {
    MCLOG(Warn, "Unable to load song from file %s", fileName.c_str());
    return;
  }

  std::string fileData((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  if (!fileData.length()) {
    MCLOG(Warn, "Unable to load song from file %s", fileName.c_str());
    return;
  }

  // Create JSON parser
  rapidjson::Document document;
  document.Parse(fileData.c_str());

  if (!document.IsObject()) {
    MCLOG(Warn, "Failure parsing JSON in file %s", fileName.c_str());
    return;
  }

  // Get instrument
  if (!document.HasMember(kInstrumentTag) || !document[kInstrumentTag].IsString()) {
    MCLOG(Warn, "Missing/invalid %s tag in song file %s", kInstrumentTag, fileName.c_str());
    return; // TODO: Maybe just load?
  }

  std::string instrumentName = document[kInstrumentTag].GetString();

  std::cout << "Song uses instrument " << instrumentName << std::endl;

  if (this->instrument == nullptr || this->instrument->GetName() != instrumentName) {
    if (!loadInstrumentCallback) {
      MCLOG(Warn, "Current instrument \'%s\' does not match song instrument \'%s\' and no "
        "callback was provided to load the correct one", this->instrument->GetName().c_str(), instrumentName.c_str());
      return;
    }

    // Prompt the caller that we need to load an instrument first
    if (!loadInstrumentCallback(instrumentName)) {
      // TODO: Could offer loading with track truncation
      MCLOG(Warn, "Unable to load specified instrument %s", instrumentName.c_str());
      return;
    }
  }

  // At this point instrument names match

  // Get tempo
  if (!document.HasMember(kTempoTag) || !document[kTempoTag].IsUint()) {
    MCLOG(Warn, "Invalid tempo in song file %s", fileName.c_str());;
    return;
  }

  auto tempo = document[kTempoTag].GetUint();
  SetBeatsPerMinute(tempo);

  // Get number of measures in song and expand instrument tracks
  if (!document.HasMember(kMeasuresTag) || !document[kMeasuresTag].IsUint()) {
    MCLOG(Warn, "Invalid number of measures in song file %s", fileName.c_str());
    return;
  }

  auto numMeasures = document[kMeasuresTag].GetUint();
  SetNumMeasures(numMeasures);

  // Get beats per measure
  if (!document.HasMember(kBeatsPerMeasureTag) || !document[kBeatsPerMeasureTag].IsUint()) {
    MCLOG(Warn, "Invalid number of beats per measure in song file %s", fileName.c_str());
    return;
  }

  auto beatsPerMeasure = document[kBeatsPerMeasureTag].GetUint();
  SetBeatsPerMeasure(beatsPerMeasure);

  // Read notes
  if (!document.HasMember(kNotesTag) || !document[kNotesTag].IsArray()) {
    MCLOG(Warn, "Invalid notes array in song file %s", fileName.c_str());
    return;
  }

  const auto& notesArray = document[kNotesTag];
  for (rapidjson::SizeType noteArrayIndex = 0; noteArrayIndex < notesArray.Size(); ++noteArrayIndex) {
    const auto& noteEntry = notesArray[noteArrayIndex];
    if (!noteEntry.HasMember(kBeatTag) || (!noteEntry[kBeatTag].IsFloat() && !noteEntry[kBeatTag].IsUint())) {
      MCLOG(Warn, "Invalid note in notes array!");
      continue;
    }

    auto beatIndex = static_cast<uint32>((noteEntry[kBeatTag].GetFloat() - 1.0f) * maxBeatSubdivisions);

    if (!noteEntry.HasMember(kTrackTag) || !noteEntry[kTrackTag].IsUint()) {
      MCLOG(Warn, "Invalid track in notes array!");
      continue;
    }
    auto trackIndex = noteEntry[kTrackTag].GetUint();

    if (!noteEntry.HasMember(kVelocityTag) || !noteEntry[kVelocityTag].IsFloat()) {
      MCLOG(Warn, "Invalid velocity in notes array!");
      continue;
    }

    auto noteVelocity = noteEntry[kVelocityTag].GetFloat();
    SetTrackNote(trackIndex, beatIndex, noteVelocity);
  }
}

void Sequencer::LoadSong(std::string fileName, std::function<bool(std::string)> loadInstrumentCallback) {
  if (fileName.compare(fileName.length() -
    kJsonTag.length(), kJsonTag.length(), kJsonTag) == 0) {
    return LoadJson(fileName, loadInstrumentCallback);
  }
  for (size_t m = 0; m < _countof(kMidiTags); ++m) {
    if (fileName.compare(fileName.length() - 
      kMidiTags[m].length(), kMidiTags[m].length(), kMidiTags[m]) == 0) {
      return LoadMidi(fileName);
    }
  }
}

bool Sequencer::Init(uint32 numMeasures, uint32 beatsPerMeasure, uint32 bpm, uint32 maxBeatSubdivisions, uint32 currBeatSubdivision) {
  this->numMeasures = numMeasures;
  this->beatsPerMeasure = beatsPerMeasure;
  this->maxBeatSubdivisions = maxBeatSubdivisions;

  SetLooping(true);
  Clear();

  Mixer::InitSingleton(kAudioBufferSize);

  sm_set_control_cb([](void* payload) {
    return reinterpret_cast<Sequencer*>(payload)->NextFrame();
  }, reinterpret_cast<void*>(this));

  // Create the reserved voice
  Mixer::Get().AddVoice();

  // Load the reserved sounds
  reservedSounds.resize(static_cast<int>(Sounds::Count));
  reservedSounds[static_cast<int>(Sounds::MetronomeFull)] = Mixer::Get().
    LoadSound(std::string("Assets\\Metronome\\seikosq50_hi.wav").c_str());
  reservedSounds[static_cast<int>(Sounds::MetronomePartial)] = Mixer::Get().
    LoadSound(std::string("Assets\\Metronome\\seikosq50_lo.wav").c_str());

  SetSubdivision(currBeatSubdivision);
  SetBeatsPerMinute(bpm);

  // TODO: Load the default instrument

  return true;
}

Sequencer::~Sequencer() {
  SDL_LockAudio();
  sm_set_control_cb(nullptr, nullptr);
  delete instrument;
  instrument = nullptr;
  Mixer::TermSingleton();
  SDL_UnlockAudio();
}
