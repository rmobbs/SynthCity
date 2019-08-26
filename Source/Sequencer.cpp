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
    UpdateInterval();
  }
}

uint32 Sequencer::UpdateInterval() {
  // Using min note value at all times so grid display does not affect playback
  // https://trello.com/c/05XYJTLP
  uint32 noteInterval = Globals::kDefaultMinNote;//currbeatSubdivision;
  if (song != nullptr) {
    noteInterval = song->GetMinNoteValue();
  }

  interval = static_cast<uint32>((Mixer::kDefaultFrequency /
    GetTempo() * 60.0) / static_cast<double>(noteInterval));
  Mixer::Get().ApplyInterval(interval);

  return interval;
}

void Sequencer::SetSubdivision(uint32 subdivision) {
  currBeatSubdivision = std::min(subdivision, Globals::kDefaultMinNote);
  if (song != nullptr) {
    currBeatSubdivision = std::min(subdivision, song->GetMinNoteValue());
  }
  // Changing the display subdivision does not affect playback
  // https://trello.com/c/05XYJTLP
  //UpdateInterval();
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

  if (!isPlaying || !instrument || !song) {
    return interval;
  }

  currPosition = nextPosition;
  // Commenting out divide so grid display doesn't affect note playback
  // https://trello.com/c/05XYJTLP
  nextPosition = currPosition + 1;// song->GetMinNoteValue() / currBeatSubdivision;

  // Still issue beat callbacks when in lead-in
  auto absCurrPosition = std::abs(currPosition);
  if ((absCurrPosition % song->GetMinNoteValue()) == 0) {
    FullNoteCallback((absCurrPosition % (song->
      GetMinNoteValue() * song->GetBeatsPerMeasure())) == 0);
  }

  if (currPosition >= 0) {
    // Handle end-of-track / looping
    if (currPosition >= static_cast<int32>(song->GetNoteCount())) {
      if (isLooping) {
        currPosition = 0;
        // Commenting out divide so grid display doesn't affect note playback
        // https://trello.com/c/05XYJTLP
        nextPosition = currPosition + 1;// song->GetMinNoteValue() / currBeatSubdivision;
        ++loopIndex;
      }
      else {
        Stop();
      }
    }

    if (isPlaying) {
      for (size_t lineIndex = 0; lineIndex < song->GetLineCount(); ++lineIndex) {
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

    // This is destructive
    // https://trello.com/c/cSv285Tr
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

      // This is destructive
      // https://trello.com/c/cSv285Tr
      NewSong();

      return true;
    }
  }
  return false;
}

void Sequencer::NewSong() {
  delete song;
  song = new Song(instrument->GetTrackCount(), Globals::kDefaultTempo,
    kDefaultNumMeasures, Song::kDefaultBeatsPerMeasure, Globals::kDefaultMinNote);
  UpdateInterval();
}

bool Sequencer::SaveSong(std::string fileName) {
  MCLOG(Info, "Saving song to file \'%s\'", fileName.c_str());

  if (!song) {
    MCLOG(Error, "Somehow there is no song in SaveSong");
    return false;
  }

  rapidjson::StringBuffer sb;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);

  auto result = song->SerializeWrite({ w });
  if (!result.first) {
    MCLOG(Error, "Unable to save song: %s", result.second.c_str());
    return false;
  }

  std::ofstream ofs(fileName);
  if (ofs.bad()) {
    MCLOG(Error, "Unable to save song to file %s ", fileName.c_str());
    return false;
  }

  std::string outputString(sb.GetString());
  ofs.write(outputString.c_str(), outputString.length());
  ofs.close();

  return true;
}

void Sequencer::LoadSongJson(std::string fileName) {
  MCLOG(Info, "Loading song from file \'%s\'", fileName.c_str());

  std::ifstream ifs(fileName);

  if (ifs.bad()) {
    MCLOG(Error, "Unable to load song from file %s", fileName.c_str());
    return;
  }

  std::string fileData((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  if (!fileData.length()) {
    MCLOG(Error, "Unable to load song from file %s", fileName.c_str());
    return;
  }

  rapidjson::Document document;
  document.Parse(fileData.c_str());

  if (!document.IsObject()) {
    MCLOG(Error, "Failure parsing JSON in file %s", fileName.c_str());
    return;
  }

  try {
    auto newSong = new Song({ document });

    if (newSong->GetMinNoteValue() > Globals::kDefaultMinNote) {
      delete newSong;
      MCLOG(Error, "Song subdivisions greater than sequencer max");
      return;
    }

    delete song;
    song = newSong;

    UpdateInterval();
  }
  catch (std::runtime_error& rte) {
    MCLOG(Error, "Failed to load song: %s", rte.what());
  }
}

// TODO: Fix MIDI loading
// https://trello.com/c/vQCRzrcm
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
          static_cast<double>(midiSource.getTimeDivision()) * Globals::kDefaultMinNote);

        if (!(beatsIndex % Globals::kDefaultMinNote)) {
          ++numMeasures;
        }
        
        //instrument->SetTrackNote(trackIndex, beatsIndex, 1.0f);
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
    // Survivable
  }
  try {
    reservedPatches[static_cast<int32>(ReservedSounds::MetronomePartial)] =
      new Patch({ }, { new WavSound("Assets\\Metronome\\seikosq50_lo.wav") });
  }
  catch (...) {
    MCLOG(Error, "Unable to load upbeat metronome WAV file");
    // Survivable
  }

  return true;
}

Sequencer::~Sequencer() {
  AudioGlobals::LockAudio();
  
  delete song;
  song = nullptr;

  delete instrument;
  instrument = nullptr;

  for (auto& reservedPatch : reservedPatches) {
    delete reservedPatch;
  }
  reservedPatches.clear();

  AudioGlobals::UnlockAudio();
}
