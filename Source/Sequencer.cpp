#include "Sequencer.h"
#include "SDL.h"
#include <iostream>
#include "Mixer.h"
#include "SDL_audio.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include "MidiSource.h"
#include "Logging.h"
#include "SynthSound.h"
#include "SerializeImpl.h"
#include "WavSound.h"
#include "Instrument.h"

static constexpr float kMetronomeVolume = 0.7f;
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
static constexpr std::string_view kNewInstrumentDefaultName("New Instrument");


enum class ReservedSounds {
  MetronomeFull,
  MetronomePartial,
  Count,
};

/* static */
Sequencer* Sequencer::singleton = nullptr;

/* static */
bool Sequencer::InitSingleton(uint32 numMeasures, uint32 beatsPerMeasure, uint32 bpm, uint32 maxBeatSubdivisions, uint32 currBeatSubdivision) {
  if (!singleton) {
    singleton = new Sequencer;
    if (singleton) {
      if (singleton->Init(numMeasures, beatsPerMeasure, bpm, maxBeatSubdivisions, currBeatSubdivision)) {
        return true;
      }
      delete singleton;
      singleton = nullptr;
    }
  }
  return false;
}

/* static */
bool Sequencer::TermSingleton() {
  delete singleton;
  singleton = nullptr;
  return true;
}

uint32 Sequencer::CalcInterval(uint32 beatSubdivision) const {
  if (currentBpm > 0 && beatSubdivision > 0) {
    return static_cast<uint32>(Mixer::kDefaultFrequency /
      currentBpm * 60.0 / static_cast<float>(beatSubdivision));
  }
  return 0;
}

void Sequencer::SetSubdivision(uint32 subdivision) {
  if (subdivision > maxBeatSubdivisions) {
    subdivision = maxBeatSubdivisions;
  }
  currBeatSubdivision = subdivision;
  interval = static_cast<int32>(CalcInterval(GetSubdivision()));
}

void Sequencer::SetBeatsPerMinute(uint32 bpm) {
  currentBpm = bpm;
  interval = static_cast<int32>(CalcInterval(GetSubdivision()));
  Mixer::Get().ApplyInterval(interval);
}

void Sequencer::PartialNoteCallback() {
  // Nothin'
}

void Sequencer::FullNoteCallback(bool isMeasure) {
  if (IsMetronomeOn()) {
    uint32 metronomeSound = static_cast<uint32>(ReservedSounds::MetronomePartial);
    if (isMeasure) {
      metronomeSound = static_cast<uint32>(ReservedSounds::MetronomeFull);
    }
    Mixer::Get().PlaySound(reservedSounds[metronomeSound], kMetronomeVolume);
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
  isLooping = looping;
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
    if (currPosition >= static_cast<int32>(instrument->tracks[trackIndex].data.size())) {
      continue;
    }

    uint8* d = instrument->tracks[trackIndex].data.data() + currPosition;
    if (*d > 0) {
      if (notePlayedCallback != nullptr) {
        notePlayedCallback(trackIndex, currPosition, notePlayedPayload);
      }
      instrument->PlayTrack(trackIndex, static_cast<float>(*d) /
        static_cast<float>(Instrument::kNoteVelocityAsUint8));
    }
  }

  return interval;
}

void Sequencer::SetNotePlayedCallback(Sequencer::NotePlayedCallback notePlayedCallback, void* notePlayedPayload) {
  this->notePlayedCallback = notePlayedCallback;
  this->notePlayedPayload = notePlayedPayload;
}

bool Sequencer::NewInstrument() {
  Instrument* newInstrument = new Instrument(std::string(kNewInstrumentDefaultName),
    GetMaxSubdivisions() * GetNumMeasures() * GetBeatsPerMeasure());
  if (newInstrument) {
    SDL_LockAudio();
    Stop();
    delete instrument;
    instrument = newInstrument;
    SDL_UnlockAudio();
    return true;
  }
  return false;
}

bool Sequencer::LoadInstrument(std::string fileName, std::string mustMatch) {
  Instrument *newInstrument = Instrument::
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
            w.Double(static_cast<float>(n) / static_cast<float>(Instrument::kNoteVelocityAsUint8));
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

void Sequencer::LoadSongMidi(std::string fileName) {
  if (!instrument) {
    MCLOG(Error, "Cannot load MIDI file without a loaded instrument");
    return;
  }

  if (!instrument->tracks.size()) {
    MCLOG(Error, "Cannot load MIDI file if instrument has no tracks");
    return;
  }

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

  if (!midiConversionParamsCallback) {
    MCLOG(Error, "MIDI file \'%s\' loaded but no callback provided", fileName.c_str());
    return;
  }

  MidiConversionParams midiConversionParams;
  if (midiConversionParamsCallback(midiSource, midiConversionParams)) {
    MidiTrack midiTrack;

    // TODO: Do this right
    static constexpr uint32 kMinMidiValue = 21;

    // Yikes
    uint32 numMeasures = 1;
    if (midiSource.CombineTracks(midiTrack, midiConversionParams.trackIndices)) {
      // Ok. We now have all the tracks we want in one track; it is only note-ons;
      // they are globally and locally time stamped
      // Now we need to iterate these and add them as notes!
      this->maxBeatSubdivisions;
      for (const auto& midiEvent : midiTrack.events) {
        auto a = static_cast<int32>(midiEvent.dataptr[1]);
        auto b = static_cast<int32>(kMinMidiValue);
        auto trackIndex = (std::max(a, b) - b) % instrument->tracks.size();
        auto beatsIndex = static_cast<uint32>(static_cast<double>(midiEvent.timeStamp) /
          static_cast<double>(midiSource.getTimeDivision()) * maxBeatSubdivisions);

        if (!(beatsIndex % maxBeatSubdivisions)) {
          ++numMeasures;
        }
        
        instrument->SetTrackNote(trackIndex, beatsIndex, 1.0f); // TODO: Need velocity
      }

      // Currently capping number of loaded measures as unlimited measures are not properly
      // clipped in ImGui and the program becomes unusable
      SetNumMeasures(20);

      MCLOG(Info, "Successfully loaded MIDI file \'%s\'", fileName.c_str());
    }
    else {
      MCLOG(Error, "Failure while combining MIDI tracks");
    }

  }
  else {
    MCLOG(Error, "Failure while converting MIDI");
  }
}

void Sequencer::LoadSongJson(std::string fileName) {
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
    instrument->SetTrackNote(trackIndex, beatIndex, noteVelocity);
  }
}

void Sequencer::LoadSong(std::string fileName) {
  if (fileName.compare(fileName.length() -
    kJsonTag.length(), kJsonTag.length(), kJsonTag) == 0) {
    return LoadSongJson(fileName);
  }
  for (size_t m = 0; m < _countof(kMidiTags); ++m) {
    if (fileName.compare(fileName.length() - 
      kMidiTags[m].length(), kMidiTags[m].length(), kMidiTags[m]) == 0) {
      return LoadSongMidi(fileName);
    }
  }
}

bool Sequencer::Init(uint32 numMeasures, uint32 beatsPerMeasure, uint32 bpm, uint32 maxBeatSubdivisions, uint32 currBeatSubdivision) {
  this->numMeasures = numMeasures;
  this->beatsPerMeasure = beatsPerMeasure;
  this->maxBeatSubdivisions = maxBeatSubdivisions;

  // Load the reserved sounds
  reservedSounds.resize(static_cast<int32>(ReservedSounds::Count));
  try {
    reservedSounds[static_cast<int32>(ReservedSounds::MetronomeFull)] =
      Mixer::Get().AddSound(new WavSound("Assets\\Metronome\\seikosq50_hi.wav"));
  }
  catch (...) {
    MCLOG(Error, "Unable to load downbeat metronome WAV file");
  }
  try {
    reservedSounds[static_cast<int32>(ReservedSounds::MetronomePartial)] =
      Mixer::Get().AddSound(new WavSound("Assets\\Metronome\\seikosq50_lo.wav"));
  }
  catch (...) {
    MCLOG(Error, "Unable to load upbeat metronome WAV file");
  }

  SetSubdivision(currBeatSubdivision);
  SetBeatsPerMinute(bpm);

  // TODO: Load the default instrument

  return true;
}

Sequencer::~Sequencer() {
  SDL_LockAudio();
  delete instrument;
  instrument = nullptr;
  SDL_UnlockAudio();
}