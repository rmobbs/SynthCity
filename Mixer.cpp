#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "logging.h"
#include "mixer.h"
#include "SDL_audio.h"
#include <vector>
#include <iostream>
#include "WavSound.h"
#include "SynthSound.h"
#include <algorithm>
#include "OddsAndEnds.h"
#include <windows.h>
#undef min
#undef max

// Upper bounds for callback
// TODO: Can just expand the buffer ...
static constexpr uint32	kMaxCallbackSampleFrames = 256;
static constexpr uint32 kMaxSimultaneousVoices = 64;

// Synth voice
static constexpr double	kSmC0 = 16.3515978312874;

static sm_control_cb control_callback = NULL;
static void* control_payload = nullptr;

static void flip_endian(Uint8 *data, int length)
{
  int i;
  for (i = 0; i < length; i += 2)
  {
    int x = data[i];
    data[i] = data[i + 1];
    data[i + 1] = x;
  }
}

void sm_set_control_cb(sm_control_cb cb, void* payload)
{
  SDL_LockAudio();
  control_callback = cb;
  control_payload = payload;
  //ticksRemaining = 0;
  SDL_UnlockAudio();
}

/* static */
Mixer *Mixer::singleton = nullptr;

Mixer::Mixer() {

}

Mixer::~Mixer() {
  SDL_LockAudio();

  if (audioDeviceId != 0) {
    SDL_CloseAudioDevice(audioDeviceId);
    audioDeviceId = 0;
  }

  for (auto& sound : sounds) {
    delete sound.second;
  }
  sounds.clear();
  for (auto& voice : voices) {
    delete voice;
  }
  voices.clear();

  SDL_UnlockAudio();
}

void Mixer::SetMasterVolume(float masterVolume) {
  this->masterVolume = masterVolume;
}

void Mixer::ReleaseSound(SoundHandle soundHandle) {
  if (soundHandle != kInvalidSoundHandle) {
    SDL_LockAudio();

    auto soundEntry = sounds.find(soundHandle);
    if (soundEntry != sounds.end()) {
      // TODO: May want to ref-count sounds
      delete soundEntry->second;
      sounds.erase(soundEntry);
    }

    SDL_UnlockAudio();
  }
}

SoundHandle Mixer::LoadSound(std::string fileName) {
  // TODO: Hash filenames and use them for a library

  WavSound* wavSound = new WavSound(fileName);
  if (wavSound == nullptr) {
    return kInvalidSoundHandle;
  }

  if (wavSound->getFrequency() != kDefaultFrequency) {
    MCLOG(Warn, "Mixer::LoadSound: %s has a frequency of %f kHz "
      "but will be played back at %f kHz", fileName.c_str(),
      static_cast<float>(wavSound->getFrequency()) / 1000.0f,
      static_cast<float>(kDefaultFrequency) / 1000.0f);
  }
  return AddSound(wavSound);
}

SoundHandle Mixer::AddSound(Sound* sound) {
  SDL_LockAudio();
  SoundHandle currSoundHandle = nextSoundHandle++;
  sounds.emplace(currSoundHandle, sound);
  SDL_UnlockAudio();

  return currSoundHandle;
}

void Mixer::ApplyInterval(uint32 ticksPerFrame) {
  if (ticksRemaining > ticksPerFrame)
    ticksRemaining = ticksPerFrame;
}

bool Mixer::Init(uint32 audioBufferSize) {
  SDL_AudioSpec as = { 0 };
  int i;

  mixbuf.resize(kMaxCallbackSampleFrames * sizeof(Sint32) * 2);

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
  {
    fprintf(stderr, "Couldn't init SDL audio: %s\n",
      SDL_GetError());
    return false;
  }

  as.freq = Mixer::kDefaultFrequency;
  as.format = AUDIO_S16SYS;
  as.channels = 2;
  as.samples = audioBufferSize;
  as.userdata = this;
  as.callback = [](void *userData, uint8 *stream, int len) {
    reinterpret_cast<Mixer*>(userData)->AudioCallback(userData, stream, len);
  };

  audioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &as, &audiospec, 0);
  if (audioDeviceId == 0)
  {
    fprintf(stderr, "Couldn't open SDL audio: %s\n",
      SDL_GetError());
    return false;
  }
  if (audiospec.format != AUDIO_S16SYS)
  {
    fprintf(stderr, "Wrong audio format!");
    return false;
  }
  SDL_PauseAudioDevice(audioDeviceId, 0);
  return true;
}

void Mixer::MixVoices(float* mixBuffer, uint32 numFrames) {
  int vi;
  // Clear the buffer
  memset(mixBuffer, 0, numFrames * sizeof(float) * 2);

  if (voices.size() > 0) {

    static constexpr float kPeakVolumeRatio = 0.7f;
    static constexpr float kVolumeEpsilon = 0.01f;

    // Mix active voices
    uint32 i = 0;
    uint32 n = voices.size();
    while (i < n) {
      Voice* v = voices[i];
      Sound* s = Mixer::Get().sounds[v->sound];

      for (uint32 f = 0; f < numFrames; ++f) {
        float samples[2] = { 0 }; // Stereo

        if (s->GetSamplesForFrame(samples, 2, v->position, v) != 2 || (v->lvol <= kVolumeEpsilon && v->rvol <= kVolumeEpsilon)) {
          v->sound = kInvalidSoundHandle;
          break;
        }

        mixBuffer[f * 2 + 0] += samples[0] * masterVolume * kPeakVolumeRatio * v->lvol;
        mixBuffer[f * 2 + 1] += samples[1] * masterVolume * kPeakVolumeRatio * v->rvol;

        v->lvol -= v->lvol * v->decay;
        v->rvol -= v->rvol * v->decay;

        ++v->position;
      }

      if (v->sound != kInvalidSoundHandle) {
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
      delete voices[v];
    }
    voices.resize(n);

    // So people can query this without locking
    numActiveVoices.store(n);
  }
}

void Mixer::WriteOutput(float *input, int16 *output, int frames)
{
  int i = 0;
  frames *= 2;	/* Stereo! */
  while (i < frames)
  {
    output[i] = input[i] * SHRT_MAX;
    ++i;
    output[i] = input[i] * SHRT_MAX;;
    ++i;
  }
}

void Mixer::AudioCallback(void *ud, Uint8 *stream, int len)
{
  /* 2 channels, 2 bytes/sample = 4 bytes/frame */
  len /= 4;
  while (len)
  {
    /* Audio processing */
    int frames = ticksRemaining;
    if (frames > kMaxCallbackSampleFrames)
      frames = kMaxCallbackSampleFrames;
    if (frames > len) {
      frames = len;
    }
    MixVoices(mixbuf.data(), frames);
    WriteOutput(mixbuf.data(), (int16 *)stream, frames);
    stream += frames * sizeof(int16) * 2;
    len -= frames;

    /* Control processing */
    ticksRemaining -= frames;
    if (!ticksRemaining)
    {
      if (control_callback) {
        ticksPerFrame = ticksRemaining = control_callback(control_payload);
      }
      else {
        ticksPerFrame = ticksRemaining = 10000;
      }
    }
  }
}

void Mixer::Play(uint32 soundHandle, float volume) {
  SDL_LockAudio();

  if (voices.size() >= kMaxSimultaneousVoices) {
    MCLOG(Error, "Currently playing max voices; sound dropped");
  }
  else {
    Voice* voice = sounds[soundHandle]->CreateVoice();

    voice->sound = soundHandle;

    voice->lvol = volume;
    voice->rvol = volume;
    
    voice->decay = 0.00004f;// (int)(decay * 16777216.0);

    voices.push_back(voice);
  }

  SDL_UnlockAudio();
}

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
void Mixer::TermSingleton() {
  delete singleton;
  singleton = nullptr;
}


