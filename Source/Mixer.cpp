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
#include <windows.h>
#undef min
#undef max

static constexpr uint32	kMaxCallbackSampleFrames = 256;
static constexpr uint32 kMaxSimultaneousVoices = 64;
static constexpr float kPeakVolumeRatio = 0.7f;

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
  SDL_LockAudio();

  if (audioDeviceId != 0) {
    SDL_CloseAudioDevice(audioDeviceId);
    audioDeviceId = 0;
  }

  for (auto& voice : voices) {
    delete voice.process;
    delete voice.sound;
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
    uint32 i = 0;
    uint32 n = voices.size();
    while (i < n) {
      auto& voice = voices[i];

      for (uint32 frame = 0; frame < numFrames; ++frame) {
        float samples[kDefaultChannels] = { 0 };

        // Get channel count samples from sound(s)
        if (voice.patch->sound->GetSamplesForFrame(samples,
          kDefaultChannels, voice.frame, voice.sound) != kDefaultChannels) {
          // TODO: Defer deletion
          delete voice.sound;
          voice.sound = nullptr;
          break;
        }

        // Apply process(es)
        if (voice.patch->process->ProcessSamples(samples,
          kDefaultChannels, voice.frame, voice.process) != true) {
          // TODO: Defer deletion
          delete voice.process;
          voice.process = nullptr;
          break;
        }

        // Add to mix buffer
        mixBuffer[frame * 2 + 0] += samples[0] * masterVolume * kPeakVolumeRatio;
        mixBuffer[frame * 2 + 1] += samples[1] * masterVolume * kPeakVolumeRatio;

        // Advance frame
        ++voice.frame;
      }

      // Trim voices whose sounds or processes have all ended
      // TODO: Defer deletion
      if (voice.sound != nullptr && voice.process != nullptr) {
        ++i;
      }
      else {
        std::swap(voices[i], voices[--n]);
      }
    }

    // Clip so we don't distort
    for (uint32 f = 0; f < numFrames; ++f) {
      mixBuffer[f * 2 + 0] = std::max(std::min(mixBuffer[f * 2 + 0], 0.7f), -0.7f);
      mixBuffer[f * 2 + 1] = std::max(std::min(mixBuffer[f * 2 + 1], 0.7f), -0.7f);
    };

    // Delete expired voices
    for (uint32 v = n; v < voices.size(); ++v) {
      delete voices[v].sound;
      delete voices[v].process;
    }
    voices.resize(n);

    // So people can query this without locking
    numActiveVoices = n;
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

void Mixer::PlayPatch(const Patch* const patch, float volume) {
  SDL_LockAudio();

  if (voices.size() >= kMaxSimultaneousVoices) {
    MCLOG(Error, "Currently playing max voices; sound dropped");
  }
  else {
    Voice voice;

    voice.patch = patch;
    voice.sound = patch->sound->CreateInstance();
    voice.process = patch->process->CreateInstance();
    voice.frame = 0;

    voices.push_back(voice);
  }

  SDL_UnlockAudio();
}

