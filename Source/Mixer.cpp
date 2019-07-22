#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Logging.h"
#include "Mixer.h"
#include "SDL_audio.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include "OddsAndEnds.h"
#include "Patch.h"
#include "Process.h"
#include "Sound.h"
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
  SDL_LockAudio();

  if (audioDeviceId != 0) {
    SDL_CloseAudioDevice(audioDeviceId);
    audioDeviceId = 0;
  }

  for (auto& voice : voices) {
    delete voice;
  }
  voices.clear();

  SDL_UnlockAudio();
}

void Mixer::SetController(Controller* controller) {
  SDL_LockAudio();
  if (this->controller) {
    this->controller->OnDisconnect();
  }
  this->controller = controller;
  if (this->controller) {
    this->controller->OnConnect();
  }
  SDL_UnlockAudio();
}

void Mixer::SetMasterVolume(float masterVolume) {
  this->masterVolume = masterVolume;
}

void Mixer::ApplyInterval(uint32 ticksPerFrame) {
  ticksRemaining = std::min(ticksRemaining, static_cast<int32>(ticksPerFrame));
}

bool Mixer::Init(uint32 audioBufferSize) {
  SDL_AudioSpec as = { 0 };

  mixbuf.resize(kMaxCallbackSampleFrames * 2);

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
    MCLOG(Error, "Couldn't init SDL audio: %s\n", SDL_GetError());
    return false;
  }

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
    // Mix active voices
    uint32 vi = 0;
    uint32 nv = voices.size();
    while (vi < nv) {
      auto& voice = *voices[vi];

      uint32 ns = 0;
      uint32 np = 0;

      for (uint32 frame = 0; frame < numFrames; ++frame) {
        float samples[kDefaultChannels] = { 0 };

        // Get samples from sound(s)
        for (auto& s : voice.sounds) {
          if (s->sound != nullptr) {
            ++ns;
            if (s->sound->GetSamplesForFrame(samples,
              kDefaultChannels, voice.frame, s) != kDefaultChannels) {
              s->sound = nullptr;
            }
          }
        }

        // Apply process(es)
        for (auto& p : voice.processes) {
          if (p->process != nullptr) {
            ++np;
            if (p->process->ProcessSamples(samples,
              kDefaultChannels, voice.frame, p) != true) {
              p->process = nullptr;
            }
          }
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
      if (ns > 0 && np > 0) {
        ++vi;
      }
      else {
        std::swap(voices[vi], voices[--nv]);
      }
    }

    // Clip so we don't distort
    for (uint32 f = 0; f < numFrames; ++f) {
      for (uint32 c = 0; c < kDefaultChannels; ++c) {
        mixBuffer[f * kDefaultChannels + c] =
          std::max(std::min(mixBuffer[f * kDefaultChannels + c], kClipMax), kClipMin);
      }
    };

    // Delete expired voices
    for (uint32 vi = nv; vi < voices.size(); ++vi) {
      voiceMap.erase(voices[vi]->voiceId);
      delete voices[vi];
    }
    voices.resize(nv);

    // So people can query this without locking
    numActiveVoices = nv;
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

void Mixer::StopVoice(int32 voiceId) {
  SDL_LockAudio();
  auto voiceMapEntry = voiceMap.find(voiceId);
  if (voiceMapEntry != voiceMap.end()) {
    auto voiceEntry = std::find(voices.begin(), voices.end(), voiceMapEntry->second);
    assert(voiceEntry != voices.end());
    voices.erase(voiceEntry);
    voiceMap.erase(voiceMapEntry);
  }
  SDL_UnlockAudio();
}

int32 Mixer::PlayPatch(const Patch* const patch, float volume) {
  SDL_LockAudio();

  if (voices.size() >= kMaxSimultaneousVoices) {
    MCLOG(Error, "Currently playing max voices; sound dropped");
    return -1;
  }
  Voice* voice = new Voice;

  voice->patch = patch;
  for (const auto& sound : patch->sounds) {
    voice->sounds.push_back(sound->CreateInstance());
  }
  for (const auto& process : patch->processes) {
    voice->processes.push_back(process->CreateInstance());
  }
  voice->frame = 0;
  voice->volume = volume;
  voice->voiceId = nextVoiceId++;

  voices.push_back(voice);

  SDL_UnlockAudio();

  return voice->voiceId;
}

