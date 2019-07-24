#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Logging.h"
#include "Mixer.h"
#include "SDL_audio.h"
#include "AudioGlobals.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include "OddsAndEnds.h"
#include "FreeList.h"
#include "Patch.h"
#include "Process.h"
#include "Sound.h"
#include "SDL.h"
#include <cassert>
#include <windows.h>
#undef min
#undef max

static constexpr uint32	kMaxCallbackSampleFrames = 256;
static constexpr uint32 kMaxSimultaneousVoices = 64;
static constexpr float kPeakVolumeRatio = 0.7f;
static constexpr float kClipMax(0.7f);
static constexpr float kClipMin(-0.7f);

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

Mixer::Voice::~Voice() {
  for (auto& process : processes) {
    delete process;
  }
  processes.clear();

  for (auto& sound : sounds) {
    delete sound;
  }
  sounds.clear();
}

Mixer::Mixer() {

}

Mixer::~Mixer() {
  AudioGlobals::LockAudio();

  StopAllVoices();

  if (AudioGlobals::GetAudioDeviceId() != 0) {
    SDL_CloseAudioDevice(AudioGlobals::GetAudioDeviceId());
    AudioGlobals::SetAudioDeviceId(0);
  }

  StaticFreeList<Voice>::Term();

  AudioGlobals::UnlockAudio();
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
  StaticFreeList<Voice>::Init(64);

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
#if 0
  // Clear the buffer
  memset(mixBuffer, 0, numFrames * sizeof(float) * 2);

  if (voices.size() > 0) {
    // Mix active voices
    auto voiceIter = voices.begin();
    while (voiceIter != voices.end()) {
      auto& voice = *(*voiceIter);

      for (uint32 frame = 0; frame < numFrames; ++frame) {
        float samples[kDefaultChannels] = { 0 };

        // Get samples from sound(s)
        auto soundIter = voice.sounds.begin();
        while (soundIter != voice.sounds.end()) {
          if ((*soundIter)->GetSamplesForFrame(samples, kDefaultChannels, voice.frame) != kDefaultChannels) {
            delete *soundIter;
            soundIter = voice.sounds.erase(soundIter);
          }
          else {
            ++soundIter;
          }
        }

        // Apply process(es)
        auto procsIter = voice.processes.begin();
        while (procsIter != voice.processes.end()) {
          if ((*procsIter)->ProcessSamples(samples, kDefaultChannels, voice.frame) != true) {
            delete* procsIter;
            procsIter = voice.processes.erase(procsIter);
          }
          else {
            ++procsIter;
          }
        }

        if (voice.processes.size() == 0 || voice.sounds.size() == 0) {
          break;
        }

        // Add to mix buffer
        for (uint32 c = 0; c < kDefaultChannels; ++c) {
          mixBuffer[frame * kDefaultChannels + c] += samples[c] * voice.volume * masterVolume * kPeakVolumeRatio;
        }

        // Advance frame
        ++voice.frame;
      }

      // Trim voices whose sounds or processes have all ended
      // NOTE: This requires every sound to have at least one process (default
      // case is thus to create a decay(0) for every sound)
      if (voice.processes.size() == 0 || voice.sounds.size() == 0) {
        StaticFreeList<Voice>::Return(&voice);

        voiceMap.erase(voice.voiceId);
        voiceIter = voices.erase(voiceIter);
      }
      else {
        ++voiceIter;
      }
    }

    // Clip so we don't distort
    for (uint32 f = 0; f < numFrames; ++f) {
      for (uint32 c = 0; c < kDefaultChannels; ++c) {
        mixBuffer[f * kDefaultChannels + c] =
          std::max(std::min(mixBuffer[f * kDefaultChannels + c], kClipMax), kClipMin);
      }
    };

    // So people can query this without locking
    numActiveVoices = voices.size();
  }
#else
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

        SoundInstance* s = (*voiceIter)->sounds[0];
        if (s->GetSamplesForFrame(samples, 2, (*voiceIter)->frame) != 2 || (*voiceIter)->volume <= kVolumeEpsilon) {
          (*voiceIter)->sounds[0]->sound = nullptr;
          break;
        }

        mixBuffer[f * 2 + 0] += samples[0] * masterVolume * kPeakVolumeRatio * (*voiceIter)->volume;
        mixBuffer[f * 2 + 1] += samples[1] * masterVolume * kPeakVolumeRatio * (*voiceIter)->volume;

        (*voiceIter)->volume -= (*voiceIter)->volume * 0.00004f;

        ++(*voiceIter)->frame;
      }

      if ((*voiceIter)->sounds[0]->sound != nullptr) {
        ++voiceIter;
      }
      else {
        StaticFreeList<Voice>::Return(*voiceIter);
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
#endif
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
  StaticFreeList<Voice>::ReturnAll();
  voices.clear();

  numActiveVoices = 0;
}

void Mixer::StopVoice(int32 voiceId) {
  AudioGlobals::LockAudio();
  auto voiceMapEntry = voiceMap.find(voiceId);
  if (voiceMapEntry != voiceMap.end()) {
    auto voiceEntry = std::find(voices.begin(), voices.end(), voiceMapEntry->second);
    assert(voiceEntry != voices.end());
    voiceMap.erase(voiceMapEntry);
    voices.erase(voiceEntry);
    numActiveVoices = voices.size();
  }
  AudioGlobals::UnlockAudio();
}

int32 Mixer::PlayPatch(const Patch* const patch, float volume) {
  if (numActiveVoices >= kMaxSimultaneousVoices) {
    MCLOG(Error, "Currently playing max voices; sound dropped");
    return -1;
  }

  AudioGlobals::LockAudio();

  Voice* voice = StaticFreeList<Voice>::Borrow();

  voice->patch = patch;
  voice->sounds.clear();
  for (const auto& sound : patch->sounds) {
    voice->sounds.push_back(sound->CreateInstance());
  }
  voice->processes.clear();
  for (const auto& process : patch->processes) {
    voice->processes.push_back(process->CreateInstance(patch->GetSoundDuration()));
  }

  voice->frame = 0;
  voice->volume = volume;
  voice->voiceId = nextVoiceId++;

  voiceMap.insert({ voice->voiceId, voice });

  voices.push_back(voice);

  AudioGlobals::UnlockAudio();

  return voice->voiceId;
}

