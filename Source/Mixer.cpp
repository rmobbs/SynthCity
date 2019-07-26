#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Logging.h"
#include "Mixer.h"
#include "SDL_audio.h"
#include "AudioGlobals.h"
#include "WavSound.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include "OddsAndEnds.h"
#include "SoundFactory.h"
#include "ProcessFactory.h"
#include "FreeList.h"
#include "Patch.h"
#include "Process.h"
#include "Sound.h"
#include "SDL.h"
#include <cassert>
#include <windows.h>
#include <array>
#include <queue>
#undef min
#undef max

static constexpr uint32	kMaxCallbackSampleFrames = 256;
static constexpr uint32 kMaxSimultaneousVoices = 64;
static constexpr float kPeakVolumeRatio = 0.7f;
static constexpr float kClipMax(0.7f);
static constexpr float kClipMin(-0.7f);
static constexpr uint32 kVoicePreallocCount = 64;

// A voice is a playing instance of a patch
class Voice {
private:
  static int32 nextVoiceId;
public:
  // Instances
  std::array<SoundInstance*, Patch::kMaxSounds * 2> sounds = { nullptr };
  uint32 curSounds = 0;
  uint32 numSounds = 0;
  std::array<ProcessInstance*, Patch::kMaxProcesses * 2> processes = { nullptr };
  uint32 curProcesses = 0;
  uint32 numProcesses = 0;

  // Frame counter
  uint32 frame = 0;

  float volume = 1.0f;

  int32 voiceId = -1;

  Voice() {

  }

  Voice(const Patch* patch, float volume)
  : volume(volume) {
    for (uint32 s = 0; s < patch->sounds.size(); ++ s) {
      sounds[s] = SoundInstanceFreeList::FreeList(patch->
        sounds[s]->GetClassHash()).Borrow(patch->sounds[s]);
    }
    curSounds = numSounds = patch->sounds.size();

    for (uint32 p = 0; p < patch->processes.size(); ++ p) {
      processes[p] = ProcessInstanceFreeList::FreeList(patch->processes[p]->
        GetClassHash()).Borrow(patch->processes[p], patch->GetSoundDuration());
    }
    curProcesses = numProcesses = patch->processes.size();

    voiceId = nextVoiceId++;
  }

  ~Voice() {

  }
};
int32 Voice::nextVoiceId = 0;

static FreeList<Voice, const Patch*, float> voiceFreeList;

std::queue<Voice*> expiredVoices;

/* static */
Mixer* Mixer::singleton = nullptr;

/* static */
bool Mixer::InitSingleton(uint32 audioBufferSize) {
  if (!singleton) {
    singleton = new Mixer;
    if (singleton) {
      if (singleton->Init(audioBufferSize)) {
        return true;
      }
      delete singleton;
      singleton = nullptr;
    }
  }
  return false;
}

/* static */
bool Mixer::TermSingleton() {
  delete singleton;
  singleton = nullptr;
  return true;
}


Mixer::Mixer() {

}

Mixer::~Mixer() {
  if (AudioGlobals::GetAudioDeviceId() != 0) {
    SDL_CloseAudioDevice(AudioGlobals::GetAudioDeviceId());
    AudioGlobals::SetAudioDeviceId(0);
  }

  voiceFreeList.Term();
}

void Mixer::SetController(Controller* controller) {
  AudioGlobals::LockAudio();
  if (this->controller) {
    this->controller->OnDisconnect();
  }
  this->controller = controller;
  if (this->controller) {
    this->controller->OnConnect();
  }
  AudioGlobals::UnlockAudio();
}

void Mixer::SetMasterVolume(float masterVolume) {
  this->masterVolume = masterVolume;
}

void Mixer::ApplyInterval(uint32 ticksPerFrame) {
  ticksRemaining = std::min(ticksRemaining, static_cast<int32>(ticksPerFrame));
}

bool Mixer::Init(uint32 audioBufferSize) {
  voiceFreeList.Init(kVoicePreallocCount);

  SDL_AudioSpec as = { 0 };

  mixbuf.resize(kMaxCallbackSampleFrames * 2);

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
    MCLOG(Error, "Couldn't init SDL audio: %s\n", SDL_GetError());
    return false;
  }

  SDL_AudioSpec audioSpec = { 0 };
  SDL_AudioDeviceID audioDeviceId = 0;
  as.freq = Mixer::kDefaultFrequency;
  as.format = AUDIO_S16SYS;
  as.channels = 2;
  as.samples = audioBufferSize;
  as.userdata = this;
  as.callback = [](void *userData, uint8 *stream, int32 length) {
    reinterpret_cast<Mixer*>(userData)->AudioCallback(userData, stream, length);
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
  return true;
}

void Mixer::MixVoices(float* mixBuffer, uint32 numFrames) {
  // Clear the buffer
  memset(mixBuffer, 0, numFrames * sizeof(float) * 2);

  if (voices.size() > 0) {

    static constexpr float kPeakVolumeRatio = 0.7f;
    static constexpr float kVolumeEpsilon = 0.01f;

    // Mix active voices
    auto voiceIter = voices.begin();
    while (voiceIter != voices.end()) {
      for (uint32 f = 0; f < numFrames; ++f) {
        float samples[2] = { 0 }; // Stereo

        auto v = *voiceIter;
        if (v->curSounds > 0) {
          for (uint32 i = 0; i < v->numSounds; ++i) {
            auto s = v->sounds[i];
            if (s != nullptr) {
              if (s->GetSamplesForFrame(samples, 2, v->frame) != 2) {
                std::swap(v->sounds[i], v->sounds[i + v->numSounds]);
                --v->curSounds;
              }
            }
          }
        }

        if (v->curProcesses > 0) {
          for (uint32 i = 0; i < v->numProcesses; ++i) {
            auto p = v->processes[i];
            if (p != nullptr) {
              if (!p->ProcessSamples(samples, 2, v->frame)) {
                std::swap(v->processes[i], v->processes[i + v->numProcesses]);
                --v->curProcesses;

                if (p->process->StopPatchOnEnd()) {
                  for (uint32 j = 0; j < v->numProcesses; ++j) {
                    if (v->processes[j]) {
                      std::swap(v->processes[j], v->processes[j + v->numProcesses]);
                    }
                  }
                  v->curProcesses = 0;
                }
              }
            }
          }
        }

        if (v->curSounds > 0 && (v->curProcesses > 0 || !v->numProcesses)) {
          mixBuffer[f * 2 + 0] += samples[0] * masterVolume * v->volume * kPeakVolumeRatio;
          mixBuffer[f * 2 + 1] += samples[1] * masterVolume * v->volume * kPeakVolumeRatio;

          ++v->frame;
        }
        else {
          break;
        }
      }

      if ((*voiceIter)->curSounds > 0 && ((*voiceIter)->curProcesses > 0 || !(*voiceIter)->numProcesses)) {
        ++voiceIter;
      }
      else {
        expiredVoices.push(*voiceIter);

        voiceMap.erase((*voiceIter)->voiceId);
        voiceIter = voices.erase(voiceIter);
      }
    }

    // Clip so we don't distort
    for (uint32 f = 0; f < numFrames; ++f) {
      mixBuffer[f * 2 + 0] = std::max(std::min(mixBuffer[f * 2 + 0], 0.7f), -0.7f);
      mixBuffer[f * 2 + 1] = std::max(std::min(mixBuffer[f * 2 + 1], 0.7f), -0.7f);
    };

    // So people can query this without locking
    numActiveVoices = voices.size();
  }
}

void Mixer::WriteOutput(float *input, int16 *output, int32 frames) {
  int32 i = 0;
  frames *= 2;	// Stereo
  while (i < frames)
  {
    output[i] = static_cast<int16>(input[i] * SHRT_MAX);
    ++i;
    output[i] = static_cast<int16>(input[i] * SHRT_MAX);
    ++i;
  }
}

void Mixer::AudioCallback(void *userData, uint8 *stream, int32 length) {
  // 2 channels, 2 bytes/sample = 4 bytes/frame
  length /= 4;

  while (length > 0) {
    // Mix and write audio
    int32 frames = std::min(std::min(ticksRemaining,
      static_cast<int32>(kMaxCallbackSampleFrames)), length);

    MixVoices(mixbuf.data(), frames);
    WriteOutput(mixbuf.data(), reinterpret_cast<int16 *>(stream), frames);

    stream += frames * sizeof(int16) * 2;
    length -= frames;

    // Update controller
    ticksRemaining -= frames;
    if (ticksRemaining <= 0) {
      if (controller) {
        ticksPerFrame = ticksRemaining = controller->NextFrame();
      }
      else {
        ticksPerFrame = ticksRemaining = 10000;
      }
    }
  }
}

void Mixer::StopAllVoices() {
  AudioGlobals::LockAudio();

  voiceFreeList.ReturnAll();
  voices.clear();
  numActiveVoices = 0;

  DrainExpiredPool();

  AudioGlobals::UnlockAudio();
}

void Mixer::StopVoice(int32 voiceId) {
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

void Mixer::DrainExpiredPool() {
  // Drain the expired pool
  while (!expiredVoices.empty()) {
    auto v = expiredVoices.front();
    expiredVoices.pop();
    for (uint32 si = 0; si < v->sounds.size(); ++si) {
      if (v->sounds[si] != nullptr) {
        SoundInstanceFreeList::FreeList(v->sounds[si]->GetSoundHash()).Return(v->sounds[si]);
        v->sounds[si] = nullptr;
      }
    }
    for (uint32 pi = 0; pi < v->processes.size(); ++pi) {
      if (v->processes[pi] != nullptr) {
        ProcessInstanceFreeList::FreeList(v->processes[pi]->GetProcessHash()).Return(v->processes[pi]);
        v->processes[pi] = nullptr;
      }
    }
    voiceFreeList.Return(v);
  }
}

int32 Mixer::PlayPatch(const Patch* patch, float volume) {
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

