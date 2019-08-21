#include "Sequencer.h"
#include "AudioGlobals.h"
#include "Globals.h"
#include <iostream>
#include "Mixer.h"
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
#include "Patch.h"
#include "ProcessDecay.h"

static constexpr float kMetronomeVolume = 0.7f;
static constexpr const char *kInstrumentTag = "Instrument";
static constexpr const char *kTempoTag = "Tempo";
static constexpr const char *kMeasuresTag = "Measures";
static constexpr const char *kBeatsPerMeasureTag = "BeatsPerMeasure";
static constexpr const char *kNotesTag = "Notes";
static constexpr const char* kTracksTag = "Tracks";
static constexpr const char* kFretTag = "Fret";
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
    return static_cast<uint32>(((Mixer::kDefaultFrequency /
      currentBpm) * 60.0) / static_cast<float>(beatSubdivision));
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

void Sequencer::SetLeadInBeats(uint32 leadInBeats) {
  this->leadInBeats = leadInBeats;
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
    Mixer::Get().PlayPatch(reservedPatches[metronomeSound], kMetronomeVolume);
  }
}

void Sequencer::Play() {
  isPlaying = true;
}

void Sequencer::Pause() {
  isPlaying = false;
}

void Sequencer::PauseKill() {
  Pause();
  Mixer::Get().StopAllVoices();
}

void Sequencer::Stop() {
  isPlaying = false;
  loopIndex = 0;
  Mixer::Get().StopAllVoices();
  SetPosition(0);// -static_cast<int32>(leadInBeats * maxBeatSubdivisions));
}

void Sequencer::SetPosition(int32 newPosition) {
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

uint32 Sequencer::GetTrackPosition(void) const {
  if (currPosition > 0) {
    return currPosition;
  }
  return 0;
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

  // Still issue beat callbacks when in lead-in
  auto absCurrPosition = std::abs(currPosition);
  if ((absCurrPosition % maxBeatSubdivisions) == 0) {
    FullNoteCallback((absCurrPosition % (maxBeatSubdivisions * GetBeatsPerMeasure())) == 0);
  }
  else {
    PartialNoteCallback();
  }

  if (currPosition >= 0) {
    // Handle end-of-track / looping
    if (currPosition >= noteCount) {
      if (isLooping) {
        currPosition = 0;
        nextPosition = currPosition + maxBeatSubdivisions / currBeatSubdivision;
        ++loopIndex;
      }
      else {
        Stop();
        return CalcInterval(GetSubdivision());
      }
    }

    for (size_t trackIndex = 0; trackIndex < instrument->tracks.size(); ++trackIndex) {
      if (instrument->tracks[trackIndex]->GetMute()) {
        continue;
      }
      auto soloTrackIndex = instrument->GetSoloTrack();
      if (soloTrackIndex != -1 && soloTrackIndex != trackIndex) {
        continue;
      }

      auto& notes = instrument->tracks[trackIndex]->GetNotes();
      if (currPosition >= static_cast<int32>(notes.size())) {
        continue;
      }

      auto d = notes.data() + currPosition;
      if (d->enabled) {
        for (auto& notePlayedCallback : notePlayedCallbacks) {
          notePlayedCallback.first(trackIndex, currPosition, notePlayedCallback.second);
        }
        instrument->PlayTrack(trackIndex);
      }
    }
  }
  return interval;
}

uint32 Sequencer::AddNotePlayedCallback(Sequencer::NotePlayedCallback notePlayedCallback, void* notePlayedPayload) {
  notePlayedCallbacks.push_back({ notePlayedCallback, notePlayedPayload });
  return notePlayedCallbacks.size() - 1;
}

void Sequencer::RemoveNotePlayedCallback(uint32 callbackId) {
  notePlayedCallbacks.erase(notePlayedCallbacks.begin() + callbackId);
}

bool Sequencer::NewInstrument() {
  Instrument* newInstrument = new Instrument(std::string(kNewInstrumentDefaultName),
    GetMaxSubdivisions() * GetNumMeasures() * GetBeatsPerMeasure());
  if (newInstrument) {
    AudioGlobals::LockAudio();
    Stop();
    delete instrument;
    instrument = newInstrument;
    AudioGlobals::UnlockAudio();
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
      AudioGlobals::LockAudio();
      Stop();
      delete instrument;
      instrument = newInstrument;
      AudioGlobals::UnlockAudio();
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
  // Version tag:string
  w.Key(Globals::kVersionTag);
  w.String(Globals::kVersionString);

  w.Key(kInstrumentTag);
  w.String(instrument->GetName().c_str());

  w.Key(kTempoTag);
  w.Uint(GetBeatsPerMinute());

  w.Key(kMeasuresTag);
  w.Uint(GetNumMeasures());

  w.Key(kBeatsPerMeasureTag);
  w.Uint(GetBeatsPerMeasure());
  
  w.Key(kTracksTag);
  w.StartArray();
  for (uint32 trackIndex = 0; trackIndex < instrument->GetTracks().size(); ++trackIndex) {
    auto track = instrument->GetTrack(trackIndex);
    if (track->GetNoteCount()) {
      w.StartObject();

      w.Key(kNotesTag);
      w.StartArray();
      
      // TODO: It's possible the song could have been saved with a different number of
      // max subdivisions ...
      for (size_t n = 0; n < track->GetNoteCount(); ++n) {
        auto& note = track->GetNote(n);
        if (note.enabled) {
          w.StartObject();

          w.Key(kBeatTag);
          w.Uint(n);
          w.Key(kFretTag);
          w.Int(note.fretIndex);

          w.EndObject();
        }
      }

      w.EndArray();
      w.EndObject();
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
        
        //instrument->SetTrackNote(trackIndex, beatsIndex, 1.0f); // TODO: Need velocity
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

  instrument->ClearNotes();

  // Version
  if (!document.HasMember(Globals::kVersionTag) || !document[Globals::kVersionTag].IsString()) {
    MCLOG(Error, "Missing/invalid version tag in song file");
    return;
  }
  std::string version = document[Globals::kVersionTag].GetString();

  if (version != std::string(Globals::kVersionString)) {
    MCLOG(Error, "Invalid song file version");
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

  // Read tracks (can have none)
  if (document.HasMember(kTracksTag) && document[kTracksTag].IsArray()) {
    const auto& tracksArray = document[kTracksTag];
    for (rapidjson::SizeType trackArrayIndex = 0; trackArrayIndex < tracksArray.Size(); ++trackArrayIndex) {
      const auto& trackEntry = tracksArray[trackArrayIndex];

      // Read notes (can have none)
      if (!trackEntry.HasMember(kNotesTag) || !trackEntry[kNotesTag].IsArray()) {
        continue;
      }

      auto track = instrument->GetTrack(trackArrayIndex);
      if (!track) {
        MCLOG(Warn, "Song references invalid track index!");
        continue;
      }

      const auto& notesArray = trackEntry[kNotesTag];
      for (rapidjson::SizeType noteArrayIndex = 0; noteArrayIndex < notesArray.Size(); ++noteArrayIndex) {
        const auto& noteEntry = notesArray[noteArrayIndex];
        if (!noteEntry.HasMember(kBeatTag) || !noteEntry[kBeatTag].IsUint()) {
          MCLOG(Warn, "Invalid beat for note in notes array!");
          continue;
        }
        if (!noteEntry.HasMember(kFretTag) || !noteEntry[kFretTag].IsInt()) {
          MCLOG(Warn, "Invalid fret for note in notes array!");
          continue;
        }

        track->SetNote(noteEntry[kBeatTag].GetUint(),
          Track::Note(true, noteEntry[kFretTag].GetInt()));
      }
    }
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
  reservedPatches.resize(static_cast<int32>(ReservedSounds::Count));
  try {
    reservedPatches[static_cast<int32>(ReservedSounds::MetronomeFull)] =
      new Patch({ }, { new WavSound("Assets\\Metronome\\seikosq50_hi.wav") });
  }
  catch (...) {
    MCLOG(Error, "Unable to load downbeat metronome WAV file");
  }
  try {
    reservedPatches[static_cast<int32>(ReservedSounds::MetronomePartial)] =
      new Patch({ }, { new WavSound("Assets\\Metronome\\seikosq50_lo.wav") });
  }
  catch (...) {
    MCLOG(Error, "Unable to load upbeat metronome WAV file");
  }

  SetSubdivision(currBeatSubdivision);
  SetBeatsPerMinute(bpm);

  return true;
}

Sequencer::~Sequencer() {
  AudioGlobals::LockAudio();
  
  delete instrument;
  instrument = nullptr;

  for (auto& reservedPatch : reservedPatches) {
    delete reservedPatch;
  }
  reservedPatches.clear();

  AudioGlobals::UnlockAudio();
}
