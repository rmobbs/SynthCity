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
#include "Song.h"

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
static constexpr uint32 kDefaultBpm = 120;
static constexpr uint32 kDefaultSubdivisions = 4;


enum class ReservedSounds {
  MetronomeFull,
  MetronomePartial,
  Count,
};

/* static */
Sequencer* Sequencer::singleton = nullptr;

/* static */
bool Sequencer::InitSingleton() {
  if (!singleton) {
    singleton = new Sequencer;
    if (singleton) {
      if (singleton->Init()) {
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

uint32 Sequencer::GetTempo() const {
  if (song != nullptr) {
    return song->GetTempo();
  }
  return kDefaultTempo;
}

void Sequencer::SetTempo(uint32 tempo) {
  if (song != nullptr) {
    song->SetTempo(tempo);

    interval = static_cast<int32>(CalcInterval(GetSubdivision()));
    Mixer::Get().ApplyInterval(interval);
  }
}

uint32 Sequencer::CalcInterval(uint32 beatSubdivision) const {
  if (GetTempo() > 0 && beatSubdivision > 0) {
    return static_cast<uint32>(((Mixer::kDefaultFrequency /
      GetTempo()) * 60.0) / static_cast<float>(beatSubdivision));
  }
  return kDefaultInterval;
}

void Sequencer::SetSubdivision(uint32 subdivision) {
  if (song != nullptr) {
    currBeatSubdivision = std::min(subdivision, song->GetBeatSubdivision());
  }
  else {
    currBeatSubdivision = std::min(subdivision, kDefaultMaxBeatSubdivisions);
  }
  interval = static_cast<int32>(CalcInterval(GetSubdivision()));
  Mixer::Get().ApplyInterval(interval);
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
  Mixer::Get().StopAllVoices();

  isPlaying = false;
  loopIndex = 0;

  SetPosition(0);
}

void Sequencer::SetPosition(int32 newPosition) {
  currPosition = newPosition;
  nextPosition = currPosition;
}

void Sequencer::SetLooping(bool looping) {
  isLooping = looping;
}

uint32 Sequencer::GetPosition() const {
  return currPosition;
}

uint32 Sequencer::GetNextPosition() const {
  return nextPosition;
}

uint32 Sequencer::NextFrame()
{
  // NOTE: Called from SDL audio callback so SM_LockAudio is in effect

  // If nothing to do, wait current subdivision (TODO: See if we can wait _minimal_ subdivision ...
  // need to fix how we're triggering metronome for that)
  if (!isPlaying || !interval || !instrument || !song) {
    return CalcInterval(GetSubdivision());
  }

  currPosition = nextPosition;
  nextPosition = currPosition + song->GetBeatSubdivision() / currBeatSubdivision;

  // Still issue beat callbacks when in lead-in
  auto absCurrPosition = std::abs(currPosition);
  if ((absCurrPosition % song->GetBeatSubdivision()) == 0) {
    FullNoteCallback((absCurrPosition % (song->
      GetBeatSubdivision() * song->GetBeatsPerMeasure())) == 0);
  }

  if (currPosition >= 0) {
    // Handle end-of-track / looping
    if (currPosition >= static_cast<int32>(song->GetNoteCount())) {
      if (isLooping) {
        currPosition = 0;
        nextPosition = currPosition + song->GetBeatSubdivision() / currBeatSubdivision;
        ++loopIndex;
      }
      else {
        Stop();
        return CalcInterval(GetSubdivision());
      }
    }

    for (size_t lineIndex = 0; lineIndex < song->GetLineCount(); ++ lineIndex) {
      // NOTE: Will lines and tracks always be 1:1?
      if (instrument->tracks[lineIndex]->GetMute()) {
        continue;
      }
      auto soloTrackIndex = instrument->GetSoloTrack();
      if (soloTrackIndex != -1 && soloTrackIndex != lineIndex) {
        continue;
      }

      auto d = song->GetLine(lineIndex).data() + currPosition;
      if (d->GetEnabled()) {
        for (auto& notePlayedCallback : notePlayedCallbacks) {
          notePlayedCallback.first(lineIndex, currPosition, notePlayedCallback.second);
        }
        instrument->PlayTrack(lineIndex);
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
  Instrument* newInstrument = new Instrument(std::string(kNewInstrumentDefaultName));
  if (newInstrument) {
    Stop();
    delete instrument;
    instrument = newInstrument;

    // TODO: Hmm
    NewSong();

    return true;
  }
  return false;
}

bool Sequencer::LoadInstrument(std::string fileName, std::string mustMatch) {
  Instrument *newInstrument = Instrument::LoadInstrument(fileName);

  if (newInstrument) {
    if (mustMatch.length() != 0 && mustMatch != newInstrument->GetName()) {
      delete newInstrument;
    }
    else {
      Stop();

      delete instrument;
      instrument = newInstrument;

      // TODO: Hmm
      NewSong();

      return true;
    }
  }
  return false;
}

void Sequencer::NewSong() {
  delete song;
  song = new Song(instrument->GetTrackCount(),
    kDefaultBpm, kDefaultNumMeasures, kDefaultBeatsPerMeasure, kDefaultSubdivisions);
  interval = static_cast<int32>(CalcInterval(GetSubdivision()));
  Mixer::Get().ApplyInterval(interval);
}

bool Sequencer::SaveSong(std::string fileName) {
  if (!song) {
    MCLOG(Warn, "Somehow there is no song in SaveSong");
    return false;
  }

  std::ofstream ofs(fileName);
  if (ofs.bad()) {
    MCLOG(Warn, "Unable to save song to file %s ", fileName.c_str());
    return false;
  }

  rapidjson::StringBuffer sb;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);

  //song->SerializeWrite({ w });

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
      for (const auto& midiEvent : midiTrack.events) {
        auto a = static_cast<int32>(midiEvent.dataptr[1]);
        auto b = static_cast<int32>(kMinMidiValue);
        auto trackIndex = (std::max(a, b) - b) % instrument->tracks.size();
        auto beatsIndex = static_cast<uint32>(static_cast<double>(midiEvent.timeStamp) /
          static_cast<double>(midiSource.getTimeDivision()) * kDefaultMaxBeatSubdivisions);

        if (!(beatsIndex % kDefaultMaxBeatSubdivisions)) {
          ++numMeasures;
        }
        
        //instrument->SetTrackNote(trackIndex, beatsIndex, 1.0f); // TODO: Need velocity
      }

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

  try {
    auto newSong = new Song({ document });

    delete song;
    song = newSong;

    interval = static_cast<int32>(CalcInterval(GetSubdivision()));
    Mixer::Get().ApplyInterval(interval);
  }
  catch (std::runtime_error& rte) {
    MCLOG(Error, "Failed to load song: %s", rte.what());
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

bool Sequencer::Init() {
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
