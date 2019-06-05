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

// Upper bounds for callback
// TODO: Can just expand the buffer ...
static constexpr uint32	kMaxCallbackSampleFrames = 256;
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
  voices.clear();

  SDL_UnlockAudio();
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

  SDL_LockAudio();
  SoundHandle currSoundHandle = nextSoundHandle++;
  sounds.emplace(currSoundHandle, wavSound);
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

Mixer::VoiceHandle Mixer::AddVoice(bool autoDestroy) {
  SDL_LockAudio();
  Mixer::VoiceHandle currVoiceHandle = nextVoiceHandle++;

  Voice voice;
  voice.autoAllocated = autoDestroy;
  voices.emplace(currVoiceHandle, voice);

  SDL_UnlockAudio();

  MCLOG(Warn, "Voice added; voice count=%d", voices.size());

  return currVoiceHandle;
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

void Mixer::MixVoices(int32* mixBuffer, uint32 numFrames) {
  int vi;
  /* Clear the buffer */
  memset(mixBuffer, 0, numFrames * sizeof(Sint32) * 2);

  // Postpone-delete voices
  std::vector<VoiceHandle> postponeDelete;

  // Active voices
  for (auto& voiceEntry : voices) {
    Voice *v = &voiceEntry.second;
    Sound *sound;

    if (v->sound == Mixer::kInvalidSoundHandle)
      continue;

    sound = Mixer::Get().sounds[v->sound];

    for (uint32 s = 0; s < numFrames; ++s)
    {
      uint16 samples[2] = { 32767, 32767}; // Stereo

      if (sound->getSamplesForFrame(samples, 2, v->position) != 2) {
        postponeDelete.push_back(voiceEntry.first);
        break;
      }

      mixBuffer[s * 2 + 0] += samples[0] * (v->lvol >> 9) >> 7;
      mixBuffer[s * 2 + 1] += samples[1] * (v->rvol >> 9) >> 7;

      v->lvol -= (v->lvol >> 8) * v->decay >> 8;
      v->rvol -= (v->rvol >> 8) * v->decay >> 8;
      
      ++v->position;
    }
  }

  for (const auto& voice : postponeDelete) {
    voices.erase(voice);
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
    if (frames > kMaxCallbackSampleFrames)
      frames = kMaxCallbackSampleFrames;
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
  if (voiceHandle == kInvalidVoiceHandle) {
    voiceHandle = AddVoice(true);
  }

  auto voiceMapEntry = voices.find(voiceHandle);
  if (voiceMapEntry != voices.end()) {
    auto& voice = voices[voiceHandle];

    voice.sound = soundHandle;
    voice.position = 0;
    volume *= volume * volume;
    voice.lvol = (int)(volume * 16777216.0);
    voice.rvol = (int)(volume * 16777216.0);
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


