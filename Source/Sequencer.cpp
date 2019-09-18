#include "Sequencer.h"
#include "AudioGlobals.h"
#include "Globals.h"
#include "MidiSource.h"
#include "Logging.h"
#include "Sound.h"
#include "SoundFactory.h"
#include "Process.h"
#include "ProcessFactory.h"
#include "SerializeImpl.h"
#include "WavSound.h"
#include "Instrument.h"
#include "Patch.h"
#include "Song.h"
#include "FreeList.h"
#include "InputState.h"
#include "View.h"
#include "SDL.h"

#include <array>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fstream>
#include <sstream>

static constexpr float kMetronomeVolume = 0.7f;
static constexpr std::string_view kMidiTags[] = { ".midi", ".mid" };
static constexpr std::string_view kJsonTag(".json");
static constexpr std::string_view kNewInstrumentDefaultName("New Instrument");
static constexpr uint32	kMaxCallbackSampleFrames = 256;
static constexpr uint32 kMaxSimultaneousVoices = 64;
static constexpr float kPeakVolumeRatio = 0.7f;
static constexpr float kClipMax(0.7f);
static constexpr float kClipMin(-0.7f);
static constexpr uint32 kVoicePreallocCount = kMaxSimultaneousVoices;
static constexpr uint32 kDefaultFrequency = 44100;
static constexpr uint32 kDefaultChannels = 2;
static constexpr float kDefaultMasterVolume = 0.7f;
static constexpr uint32 kDefaultAudioBufferSize = 2048;

static constexpr char kGameplayKeys[] = { 'a', 's', 'd', 'f' };

// A voice is a playing instance of a patch
class Voice {
private:
  static int32 nextVoiceId;
public:
  // Instances
  std::array<SoundInstance*, Patch::kMaxSounds> sounds = { nullptr };
  uint32 bitSounds = 0;
  uint32 numSounds = 0;
  std::array<ProcessInstance*, Patch::kMaxProcesses> processes = { nullptr };
  uint32 bitProcesses = 0;
  uint32 numProcesses = 0;

  // Frame counter
  uint32 frame = 0;

  float volume = 1.0f;

  int32 voiceId = -1;

  Voice() {

  }

  Voice(const Patch* patch, float volume)
    : volume(volume) {
    bitSounds = 0;
    for (uint32 s = 0; s < patch->sounds.size(); ++s) {
      bitSounds |= 1 << s;
      sounds[s] = SoundInstanceFreeList::FreeList(patch->
        sounds[s]->GetClassHash()).Borrow(patch->sounds[s]);
    }
    numSounds = patch->sounds.size();

    bitProcesses = 0;
    for (uint32 p = 0; p < patch->processes.size(); ++p) {
      bitProcesses |= 1 << p;
      processes[p] = ProcessInstanceFreeList::FreeList(patch->processes[p]->
        GetClassHash()).Borrow(patch->processes[p], patch->GetSoundDuration());
    }
    numProcesses = patch->processes.size();

    voiceId = nextVoiceId++;
  }

  ~Voice() {

  }
};


int32 Voice::nextVoiceId = 0;

static FreeList<Voice, const Patch*, float> voiceFreeList;

std::queue<Voice*> expiredVoices;

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

Sequencer::Sequencer() 
  : gameInput({ 'a', 's', 'd', 'f' }) {

}

bool Sequencer::Init() {
  voiceFreeList.Init(kVoicePreallocCount);

  SDL_AudioSpec as = { 0 };

  mixbuf.resize(kMaxCallbackSampleFrames * 2);

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
    MCLOG(Error, "Couldn't init SDL audio: %s\n", SDL_GetError());
    return false;
  }

  SDL_AudioSpec audioSpec = { 0 };
  SDL_AudioDeviceID audioDeviceId = 0;
  as.freq = kDefaultFrequency;
  as.format = AUDIO_S16SYS;
  as.channels = 2;
  as.samples = kDefaultAudioBufferSize;
  as.userdata = this;
  as.callback = [](void *userData, uint8 *stream, int32 length) {
    reinterpret_cast<Sequencer*>(userData)->AudioCallback(userData, stream, length);
  };

  audioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &as, &audioSpec, 0);
  if (audioDeviceId == 0) {
    MCLOG(Error, "Couldn't open SDL audio: %s\n", SDL_GetError());
    return false;
  }

  AudioGlobals::SetAudioDeviceId(audioDeviceId);

  if (audioSpec.format != AUDIO_S16SYS) {
    MCLOG(Error, "Wrong audio format!");
    return false;
  }

  SDL_PauseAudioDevice(audioDeviceId, 0);

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

  if (AudioGlobals::GetAudioDeviceId() != 0) {
    SDL_CloseAudioDevice(AudioGlobals::GetAudioDeviceId());
    AudioGlobals::SetAudioDeviceId(0);
  }

  voiceFreeList.Term();
}

uint32 Sequencer::GetFrequency() const {
  return kDefaultFrequency;
}

uint32 Sequencer::UpdateInterval() {
  if (song != nullptr) {
    // Using min note value at all times so grid display does not affect playback
    // https://trello.com/c/05XYJTLP
    interval = static_cast<uint32>((kDefaultFrequency / song->
      GetTempo() * 60.0) / static_cast<double>(song->GetMinNoteValue()));
  }
  else {
    interval = kDefaultInterval;
  }

  // Don't wait for a longer interval to apply a shorter interval
  ticksRemaining = std::min(ticksRemaining, static_cast<int32>(interval));

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

void Sequencer::PlayMetronome(bool downBeat) {
  uint32 metronomeSound = static_cast<uint32>(ReservedSounds::MetronomePartial);
  if (downBeat) {
    metronomeSound = static_cast<uint32>(ReservedSounds::MetronomeFull);
  }

  PlayPatch(reservedPatches[metronomeSound], kMetronomeVolume);
}

void Sequencer::Play() {
  AudioGlobals::LockAudio();
  isPlaying = true;
  songStartFrame = frameCounter;
  ticksRemaining = 0;
  AudioGlobals::UnlockAudio();
}

void Sequencer::Pause() {
  isPlaying = false;
}

void Sequencer::PauseKill() {
  Pause();
  StopAllVoices();
}

void Sequencer::Stop() {
  StopAllVoices();

  isPlaying = false;
  loopIndex = 0;
  beatTime = 0.0f;

  SetPosition(0);
}

void Sequencer::SetPosition(uint32 newPosition) {
  nextBeat = currBeat = newPosition;
}

uint32 Sequencer::GetPosition() const {
  return currBeat;
}

uint32 Sequencer::GetNextPosition() const {
  return nextBeat;
}

void Sequencer::ResetFrameCounter() {
  AudioGlobals::LockAudio();
  nextFrame = currFrame = 0;
  AudioGlobals::UnlockAudio();
}

uint32 Sequencer::NextFrame()
{
  currFrame = nextFrame++;

  auto view = View::GetCurrentView();

  if (view != nullptr) {
    view->OnFrame(currFrame);
  }

  if (!instrument || !song || !isPlaying) {
    return interval;
  }

  currBeat = nextBeat++;

  if (view != nullptr) {
    view->OnBeat(currBeat);
  }

  return interval;
}

void Sequencer::MixVoices(float* mixBuffer, uint32 numFrames) {
  uint32 maxFrames = 0;

  // Fill remainder with zeroes
  std::memset(mixBuffer, 0, numFrames * sizeof(float) * 2);

  if (voices.size() > 0) {
    // Mix active voices
    auto voiceIter = voices.begin();
    while (voiceIter != voices.end()) {
      auto v = *voiceIter;

      uint32 outFrame = 0;
      while (outFrame < numFrames) {
        float samples[2] = { 0 }; // Stereo

        for (uint32 i = 0; i < v->numSounds; ++i) {
          if (v->bitSounds & (1 << i)) {
            if (v->sounds[i]->GetSamplesForFrame(samples, 2, v->frame) != 2) {
              v->bitSounds &= ~(1 << i);
            }
          }
        }

        for (uint32 i = 0; i < v->numProcesses; ++i) {
          if (v->bitProcesses & (1 << i)) {
            if (!v->processes[i]->ProcessSamples(samples, 2, v->frame)) {
              v->bitProcesses &= ~(1 << i);
            }
          }
        }

        if (v->bitSounds == 0 || (v->numProcesses > 0 && v->bitProcesses == 0)) {
          break;
        }

        mixBuffer[outFrame * 2 + 0] += samples[0] * masterVolume * v->volume * kPeakVolumeRatio;
        mixBuffer[outFrame * 2 + 1] += samples[1] * masterVolume * v->volume * kPeakVolumeRatio;

        ++v->frame;
        ++outFrame;
      }

      if (maxFrames < outFrame) {
        maxFrames = outFrame;
      }

      if (v->bitSounds == 0 || (v->numProcesses > 0 && v->bitProcesses == 0)) {
        expiredVoices.push(*voiceIter);
        voiceMap.erase((*voiceIter)->voiceId);
        voiceIter = voices.erase(voiceIter);
      }
      else {
        ++voiceIter;
      }
    }

    // Clip so we don't distort
    for (uint32 frameIndex = 0; frameIndex < maxFrames; ++frameIndex) {
      mixBuffer[frameIndex * 2 + 0] = std::max(std::min(mixBuffer[frameIndex * 2 + 0], 0.7f), -0.7f);
      mixBuffer[frameIndex * 2 + 1] = std::max(std::min(mixBuffer[frameIndex * 2 + 1], 0.7f), -0.7f);
    }

    // So people can query this without locking
    numActiveVoices = voices.size();
  }
}

void Sequencer::WriteOutput(float *input, int16 *output, int32 frames) {
  int32 i = 0;
  frames *= 2;	// Stereo
  while (i < frames) {
    output[i] = static_cast<int16>(input[i] * SHRT_MAX);
    ++i;
    output[i] = static_cast<int16>(input[i] * SHRT_MAX);
    ++i;
  }
}

void Sequencer::AudioCallback(void *userData, uint8 *stream, int32 length) {
  // 2 channels, 2 bytes/sample = 4 bytes/frame
  length /= 4;

  while (length > 0) {
    if (isPlaying) {
      beatTime = static_cast<float>(frameCounter - songStartFrame) /
        (static_cast<float>(interval) * static_cast<float>(song->GetMinNoteValue()));

      // TODO: This should only happen in the game mode audio callback
      if (isGameplayMode) {
        std::array<float, GameGlobals::kNumGameplayLines> presses = { 0.0f };
        std::array<float, GameGlobals::kNumGameplayLines> releases = { 0.0f };

        gameInput.TakeSnapshot(beatTime, presses, releases);

        for (size_t gameLineIndex = 0; gameLineIndex < GameGlobals::kNumGameplayLines; ++gameLineIndex) {
          if (presses[gameLineIndex]) {
            // Trigger the sound associated with the line
            instrument->PlayTrack(gameLineIndex);
          }
        }
      }
    }

    if (ticksRemaining <= 0) {
      ticksRemaining = NextFrame();
    }

    int32 frames = std::min(std::min(ticksRemaining,
      static_cast<int32>(kMaxCallbackSampleFrames)), length);

    ticksRemaining -= frames;
    frameCounter += frames;

    // Mix and write audio
    MixVoices(mixbuf.data(), frames);
    WriteOutput(mixbuf.data(), reinterpret_cast<int16 *>(stream), frames);

    stream += frames * sizeof(int16) * 2;
    length -= frames;
  }
}

void Sequencer::SetIntroBeats(uint32 introBeatCount) {
  assert(!isPlaying);
  this->introBeatCount = introBeatCount;
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
  
  View::GetCurrentView()->OnSongUpdated();
  
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

    if (newSong->GetInstrumentName() != instrument->GetName()) {
      if (!loadInstrumentCallback) {
        MCLOG(Error, "Song requires instrument \'%s\' but no load instrument "
          "callback was provided", newSong->GetInstrumentName().c_str());
        delete newSong;
        return;
      }
      if (!loadInstrumentCallback(newSong->GetInstrumentName())) {
        MCLOG(Error, "Incorrect instrument loaded for song");
        delete newSong;
        return;
      }
    }

    delete song;
    song = newSong;

    View::GetCurrentView()->OnSongUpdated();

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

void Sequencer::StopAllVoices() {
  AudioGlobals::LockAudio();

  voiceFreeList.ReturnAll();
  voices.clear();
  numActiveVoices = 0;
  frameCounter = 0;

  DrainExpiredPool();

  AudioGlobals::UnlockAudio();
}

void Sequencer::StopVoice(int32 voiceId) {
  AudioGlobals::LockAudio();
  auto voiceMapEntry = voiceMap.find(voiceId);
  if (voiceMapEntry != voiceMap.end()) {
    auto voiceEntry = std::find(voices.begin(), voices.end(), voiceMapEntry->second);
    assert(voiceEntry != voices.end());
    voices.erase(voiceEntry);

    voiceFreeList.Return(voiceMapEntry->second);
    voiceMap.erase(voiceMapEntry);

    numActiveVoices = voices.size();

    DrainExpiredPool();
  }
  AudioGlobals::UnlockAudio();
}

void Sequencer::DrainExpiredPool() {
  // Drain the expired pool
  while (!expiredVoices.empty()) {
    auto v = expiredVoices.front();
    expiredVoices.pop();
    for (uint32 si = 0; si < v->numSounds; ++si) {
      assert(v->sounds[si] != nullptr);
      SoundInstanceFreeList::FreeList(v->sounds[si]->GetSoundHash()).Return(v->sounds[si]);
    }
    for (uint32 pi = 0; pi < v->numProcesses; ++pi) {
      assert(v->processes[pi] != nullptr);
      ProcessInstanceFreeList::FreeList(v->processes[pi]->GetProcessHash()).Return(v->processes[pi]);
    }
    voiceFreeList.Return(v);
  }
}

int32 Sequencer::PlayPatch(const Patch* patch, float volume) {
  if (numActiveVoices >= kMaxSimultaneousVoices) {
    MCLOG(Error, "Currently playing max voices; sound dropped");
    return -1;
  }

  AudioGlobals::LockAudio();

  DrainExpiredPool();

  Voice* voice = voiceFreeList.Borrow(patch, volume);

  voiceMap.insert({ voice->voiceId, voice });
  voices.push_back(voice);
  numActiveVoices = voices.size();

  AudioGlobals::UnlockAudio();

  return voice->voiceId;
}
