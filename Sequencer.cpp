#include "Sequencer.h"
#include "SDL.h"
#include <iostream>
#include "mixer.h"
#include "SDL_audio.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "tinyxml2.h"

static constexpr int kAudioBufferSize = 2048;
static constexpr float kMetronomeVolume = 0.7f;

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

Sequencer::Track::~Track() {

}

void Sequencer::Track::ClearNotes() {
  data.clear();
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
  auto soundIndex = sm_load(fileName.c_str());
  if (soundIndex != -1) {
    auto trackIndex = tracks.size();
    tracks.resize(trackIndex + 1);

    auto voiceIndex = sm_addvoice();

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
  sm_play(track.voiceIndex, track.soundIndex, vel * track.lvol, vel * track.rvol);
  sm_decay(track.voiceIndex, track.decay);
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
void Sequencer::SetTrackNote(uint32 trackIndex, uint32 noteIndex, uint8 noteValue) {
  if (instrument != nullptr) {
    if (trackIndex < instrument->tracks.size()) {

      SDL_LockAudio();
      if (noteIndex >= instrument->tracks[trackIndex].data.size()) {
        instrument->tracks[trackIndex].data.resize(noteIndex + 1, 0);
      }
      instrument->tracks[trackIndex].data[noteIndex] = noteValue;
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
  sm_force_interval(interval);
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
    sm_play(static_cast<uint32>(Voices::Reserved1),
      metronomeSound, kMetronomeVolume, kMetronomeVolume);
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
      nextPosition = currPosition;
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

void Sequencer::SetNotePlayedCallback(Sequencer::NotePlayedCallback notePlayedCallback, void* notePlayedPayload) {
  this->notePlayedCallback = notePlayedCallback;
  this->notePlayedPayload = notePlayedPayload;
}

void Sequencer::Clear() {
  SDL_LockAudio();
  if (instrument != nullptr) {
    instrument->Clear();
  }
  SDL_UnlockAudio();
}

void Sequencer::LoadInstrument(std::string fileName) {
  Instrument *newInstrument = Sequencer::Instrument::
    LoadInstrument(fileName, GetMaxSubdivisions() * GetNumMeasures() * GetBeatsPerMeasure());

  if (newInstrument) {
    delete instrument;
    instrument = newInstrument;
  }
}

bool Sequencer::Init(uint32 numMeasures, uint32 beatsPerMeasure, uint32 bpm, uint32 maxBeatSubdivisions, uint32 currBeatSubdivision) {
  this->numMeasures = numMeasures;
  this->beatsPerMeasure = beatsPerMeasure;
  this->maxBeatSubdivisions = maxBeatSubdivisions;

  sm_set_control_cb([](void* payload) {
    return reinterpret_cast<Sequencer*>(payload)->NextFrame();
  },
  reinterpret_cast<void*>(this));

  SetLooping(true);
  Clear();

  // Create the reserved voices
  if (sm_open(kAudioBufferSize, static_cast<int>(Voices::ReservedCount)) < 0) {
    return false;
  }

  // Load the reserved sounds
  sm_load(static_cast<int>(Sounds::MetronomeFull),
    std::string("Instrument\\Metronome\\seikosq50_hi.wav").c_str());
  sm_load(static_cast<int>(Sounds::MetronomePartial),
    std::string("Instrument\\Metronome\\seikosq50_lo.wav").c_str());

  SetSubdivision(currBeatSubdivision);
  SetBeatsPerMinute(bpm);

  // TODO: Load the default instrument

  return true;
}

Sequencer::~Sequencer() {
  sm_set_control_cb(nullptr, nullptr);
  delete instrument;
  instrument = nullptr;
}
