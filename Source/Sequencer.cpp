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
static constexpr double kBpmToBps = 1.0 / 60.0;
static constexpr auto kInvalidTimePoint = std::chrono::high_resolution_clock::time_point::max();

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

Sequencer::Sequencer() {

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
    reinterpret_cast<Sequencer*>(userData)->AudioCallback(stream, length);
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

void Sequencer::SetTempo(uint32 tempo) {
  this->tempo = tempo;

  interval = static_cast<uint32>(kDefaultFrequency /
    (static_cast<double>(tempo) * kBpmToBps *
     static_cast<double>(Globals::kDefaultMinNote)));

  // Don't wait for a longer interval to apply a shorter interval
  ticksRemaining = std::min(ticksRemaining, static_cast<int32>(interval));
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
  AudioGlobals::UnlockAudio();
}

void Sequencer::Pause() {
  isPlaying = false;
  clockTimePoint = kInvalidTimePoint;
}

void Sequencer::PauseKill() {
  Pause();
  StopAllVoices();
}

void Sequencer::Stop() {
  clockBeatTime = 0.0;
  frameBeatTime = -Globals::kInverseDefaultMinNote;
  isPlaying = false;
  SetPosition(0);
  clockTimePoint = kInvalidTimePoint;
}

void Sequencer::StopKill() {
  Stop();
  StopAllVoices();
}

void Sequencer::Loop() {
  clockBeatTime = 0.0;
  frameBeatTime = -Globals::kInverseDefaultMinNote;
  currBeat = 0;
  nextBeat = 1;
  clockTimePoint = kInvalidTimePoint;
}

double Sequencer::GetClockBeatTime() {
  if (isPlaying && clockTimePoint != kInvalidTimePoint) {
    auto timePoint = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> delta = timePoint - clockTimePoint;
    clockBeatTime += delta.count() * static_cast<double>(tempo) * kBpmToBps;
    clockTimePoint = timePoint;
  }

  return clockBeatTime;
}

double Sequencer::GetFrameBeatTime() {
  return frameBeatTime;
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

uint32 Sequencer::NextFrame()
{
  if (!isPlaying) {
    return interval;
  }

  // Wait for the callback to ensure perfect frame-zero sync
  if (clockTimePoint == kInvalidTimePoint) {
    clockTimePoint = std::chrono::high_resolution_clock::now();
  }

  frameTimePoint = std::chrono::high_resolution_clock::now();
  frameBeatTime += Globals::kInverseDefaultMinNote;

  currBeat = nextBeat++;

  if (listener != nullptr) {
    listener->OnBeat(currBeat);
  }

  return interval;
}

void Sequencer::MixVoices(float* mixBuffer, uint32 numFrames) {
  uint32 maxFrames = 0;

  // Fill buffer with zeroes
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

void Sequencer::AudioCallback(uint8 *stream, int32 length) {
  // 2 channels, 2 bytes/sample = 4 bytes/frame
  length /= 4;

  int32 frames = length;
  while (frames > 0) {
    int32 chunk = std::min(std::min(ticksRemaining,
      static_cast<int32>(kMaxCallbackSampleFrames)), frames);

    ticksRemaining -= chunk;
    if (ticksRemaining <= 0) {
      ticksRemaining = NextFrame();
    }

    // Mix and write audio
    MixVoices(mixbuf.data(), chunk);
    WriteOutput(mixbuf.data(), reinterpret_cast<int16 *>(stream), chunk);

    stream += chunk * sizeof(int16) * 2;
    frames -= chunk;
  }
}

void Sequencer::SetListener(Listener* newListener) {
  AudioGlobals::LockAudio();
  listener = newListener;
  AudioGlobals::UnlockAudio();
}

void Sequencer::StopAllVoices() {
  AudioGlobals::LockAudio();

  voiceFreeList.ReturnAll();
  voices.clear();
  numActiveVoices = 0;

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
