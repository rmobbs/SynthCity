/*
 * smixer.c - Very simple audio mixer for SDL
 *
 * (C) David Olofson, 2003, 2006
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mixer.h"
#include "SDL_audio.h"
#include <vector>
#include <iostream>

/*
 * Maximum number of sample frames that will ever be
 * processed in one go. Audio processing callbacks
 * rely on never getting a 'frames' argument greater
 * than this value.
 */
#define	SM_MAXFRAGMENT	256

#define	SM_C0		16.3515978312874

#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

static sm_control_cb control_callback = NULL;
static void* control_payload = nullptr;

static constexpr float kSilenceThresholdIntro = 0.01f;
static constexpr float kSilenceThresholdOutro = 0.50f;

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

void Mixer::Sound::Unload() {
  SDL_LockAudio();
  if (data)
  {
    if (length) {
      SDL_FreeWAV(data);
    }
    else {
      free(data);
    }
    data = nullptr;
  }
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
  SDL_UnlockAudio();

  for (auto& sound : sounds) {
    delete sound.second;
  }
  sounds.clear();
  voices.clear();
}

void Mixer::ReleaseSound(Mixer::SoundHandle soundHandle) {
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

Mixer::SoundHandle Mixer::LoadSound(std::string fileName) {
  // TODO: Hash filenames and use them for a library
  Sound sound;

  SDL_AudioSpec spec;
  if (SDL_LoadWAV(fileName.c_str(), &spec, &sound.data, &sound.length) == nullptr)
  {
    //SDL_UnlockAudio();
    return kInvalidSoundHandle;
  }

  if (spec.freq != 44100) {
    std::cerr << "Warning: file " << fileName << " is " << spec.freq << "kHz, not 44.1kHz" << std::endl;
  }

  if (spec.channels != 1)
  {
    std::cerr << "File " << fileName << " has " <<
      std::to_string(spec.channels) << " channels; only mono sounds are supported" << std::endl;
    return kInvalidSoundHandle;
  }

  switch (spec.format)
  {
  case AUDIO_S16LSB:
  case AUDIO_S16MSB:
    if (spec.format != AUDIO_S16SYS) {
      int i;
      for (i = 0; i < sound.length; i += 2)
      {
        int x = sound.data[i];
        sound.data[i] = sound.data[i + 1];
        sound.data[i + 1] = x;
      }
    }
    break;
  default:
    std::cerr << "File " << fileName << " has sample format " <<
      std::to_string(spec.format) << "; this is unsupported " << std::endl;
    return kInvalidSoundHandle;
  }

  sound.filename = fileName;
  sound.channels = 1;
  sound.readbuf = sound.data;

  // Skip any inaudible intro ...
  // TODO: this should really be a pre-processing step
  auto frameSize = sizeof(uint16) * sound.channels;
  auto audibleOffset = sound.readbuf;
  auto audibleLength = static_cast<int32>(sound.length);
  while (audibleLength > 0) {
    int32 sum = 0;
    for (int c = 0; c < sound.channels; ++c) {
      sum += reinterpret_cast<int16*>(audibleOffset)[c];
    }

    if ((static_cast<float>(sum) / SHRT_MAX) > kSilenceThresholdIntro) {
      break;
    }

    audibleOffset += frameSize;
    audibleLength -= frameSize;
  }

  if (audibleLength > 0) {
    sound.readbuf = audibleOffset;
    sound.length = audibleLength;
  }
  else {
    std::cerr << "Sound " << fileName << " is totally inaudible by current metric; not trimming" << std::endl;
  }

  // Length is in bytes, we're counting 16-bit samples
  sound.length /= 2;

  SDL_LockAudio();
  SoundHandle currSoundHandle = nextSoundHandle++;
  sounds.emplace(currSoundHandle, new Sound(std::move(sound)));
  SDL_UnlockAudio();

  return currSoundHandle;
}

void Mixer::ReleaseVoice(VoiceHandle voiceHandle) {
  if (voiceHandle != kInvalidVoiceHandle) {
    SDL_LockAudio();

    auto voiceEntry = voices.find(voiceHandle);
    if (voiceEntry != voices.end()) {
      voices.erase(voiceEntry);
    }
    SDL_UnlockAudio();
  }
}

void Mixer::ApplyInterval(uint32 ticksPerFrame) {
  if (ticksRemaining > ticksPerFrame)
    ticksRemaining = ticksPerFrame;
}

Mixer::VoiceHandle Mixer::AddVoice() {
  SDL_LockAudio();
  Mixer::VoiceHandle currVoiceHandle = nextVoiceHandle++;
  voices.emplace(currVoiceHandle, Voice());
  SDL_UnlockAudio();

  return currVoiceHandle;
}

bool Mixer::Init(uint32 audioBufferSize) {
  SDL_AudioSpec as = { 0 };
  int i;

  mixbuf.resize(SM_MAXFRAGMENT * sizeof(Sint32) * 2);

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
  {
    fprintf(stderr, "Couldn't init SDL audio: %s\n",
      SDL_GetError());
    return false;
  }

  as.freq = 44100;
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

void Mixer::MixVoices(int32* mixBuffer, uint32 numFrames) {
  int vi, s;
  /* Clear the buffer */
  memset(mixBuffer, 0, numFrames * sizeof(Sint32) * 2);

  /* For each voice... */
  for (auto& voiceEntry : voices) {
    Mixer::Voice *v = &voiceEntry.second;
    Mixer::Sound *sound;
    if (v->sound == Mixer::kInvalidSoundHandle)
      continue;
    sound = Mixer::Get().sounds[v->sound];
    if (sound->length)
    {
      /* Sampled waveform */
      int16 *d = (int16 *)sound->readbuf;;
      for (s = 0; s < numFrames; ++s)
      {
        int v1715;
        if (v->position >= sound->length)
        {
          v->sound = -1;
          break;
        }
        v1715 = v->lvol >> 9;
        mixBuffer[s * 2] += d[v->position] * v1715 >> 7;
        v1715 = v->rvol >> 9;
        mixBuffer[s * 2 + 1] += d[v->position] * v1715 >> 7;
        v->lvol -= (v->lvol >> 8) * v->decay >> 8;
        v->rvol -= (v->rvol >> 8) * v->decay >> 8;
        ++v->position;
      }
    }
    else
    {
      /* Synth voice */
      double f = SM_C0 * pow(2.0f, sound->pitch / 12.0);
      double ff = M_PI * 2.0f * f / 44100.0f;
      double fm = sound->fm * 44100.0f / f;
      for (s = 0; s < numFrames; ++s)
      {
        int v1715;
        float mod = sin(v->position * ff) * fm;
        int w = sin((v->position + mod) * ff) * 32767.0f;
        v1715 = v->lvol >> 9;
        mixBuffer[s * 2] += w * v1715 >> 7;
        v1715 = v->rvol >> 9;
        mixBuffer[s * 2 + 1] += w * v1715 >> 7;
        v->lvol -= (v->lvol >> 8) * v->decay >> 8;
        v->rvol -= (v->rvol >> 8) * v->decay >> 8;
        ++v->position;
      }
      v->lvol -= 16;
      if (v->lvol < 0)
        v->lvol = 0;
      v->rvol -= 16;
      if (v->rvol < 0)
        v->rvol = 0;
    }
  }
}

/* Convert from 8:24 (32 bit) to 16 bit (stereo) */
void Mixer::WriteOutput(Sint32 *input, int16 *output, int frames)
{
  int i = 0;
  frames *= 2;	/* Stereo! */
  while (i < frames)
  {
    output[i] = input[i] >> 8;
    ++i;
    output[i] = input[i] >> 8;
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
    if (frames > SM_MAXFRAGMENT)
      frames = SM_MAXFRAGMENT;
    if (frames > len) {
      frames = len;
    }
    Mixer::Get().MixVoices(reinterpret_cast<int32*>(mixbuf.data()), frames);
    WriteOutput(reinterpret_cast<int32*>(mixbuf.data()), (int16 *)stream, frames);
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

void Mixer::Play(uint32 voiceHandle, uint32 soundHandle, float volume) {
  auto voiceEntry = voices.find(voiceHandle);
  if (voiceEntry != voices.end()) {
    voices[voiceHandle].sound = soundHandle;
    voices[voiceHandle].position = 0;
    volume *= volume * volume;
    voices[voiceHandle].lvol = (int)(volume * 16777216.0);
    voices[voiceHandle].rvol = (int)(volume * 16777216.0);
  }
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


