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

#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

static std::vector<Mixer::Sound> sounds;
static std::vector<Mixer::Voice> voices;
static SDL_AudioSpec audiospec;

/* Internal mixing buffer; 0 dB level is at 24 bits peak. */
static Sint32 *mixbuf = NULL;

/* Current control interval duration */
static int interval = 0;

/* Sample frames left until the next control tick */
static int next_tick = 0;

static sm_control_cb control_callback = NULL;
static void* control_payload = nullptr;
static sm_audio_cb audio_callback = NULL;
static sm_subdivision_callback subdivision_callback = nullptr;
static void* subdivision_payload = nullptr;

static constexpr float kSilenceThresholdIntro = 0.01f;
static constexpr float kSilenceThresholdOutro = 0.50f;

int sm_get_interval(void)
{
  return interval;
}

int sm_get_next_tick(void)
{
  return next_tick;
}

SDL_AudioDeviceID audioDeviceId = 0;

/* Start playing 'sound' on 'voice' at L/R volumes 'lvol'/'rvol' */
void sm_play(unsigned voice, unsigned sound, float lvol, float rvol)
{
  if (voice >= voices.size() || sound >= sounds.size())
    return;
  voices[voice].sound = sound;
  voices[voice].position = 0;
  lvol *= lvol * lvol;
  rvol *= rvol * rvol;
  voices[voice].lvol = (int)(lvol * 16777216.0);
  voices[voice].rvol = (int)(rvol * 16777216.0);
  if (!sounds[sound].length)
  {
    float decay = sounds[sound].decay;
    decay *= decay;
    decay *= 0.00001f;
    voices[voice].decay = (int)(decay * 16777216.0);
  }
}


void sm_decay(unsigned voice, float decay)
{
  if (voice >= voices.size())
    return;

  int sound = voices[voice].sound;
  if (sound < 0)
    return;
  if (!sounds[sound].length)
    decay += sounds[sound].decay;
  decay *= decay;
  decay *= 0.00001f;
  voices[voice].decay = (int)(decay * 16777216.0);
}


/* Mix all voices into a 32 bit (8:24) stereo buffer */
static void sm_mixer(Sint32 *buf, int frames)
{
  int vi, s;
  /* Clear the buffer */
  memset(buf, 0, frames * sizeof(Sint32) * 2);

  /* For each voice... */
  for (vi = 0; vi < voices.size(); ++vi)
  {
    Mixer::Voice *v = &voices[vi];
    Mixer::Sound *sound;
    if (v->sound < 0)
      continue;
    sound = &sounds[v->sound];
    if (sound->length)
    {
      /* Sampled waveform */
      int16 *d = (int16 *)sound->readbuf;;
      for (s = 0; s < frames; ++s)
      {
        int v1715;
        if (v->position >= sound->length)
        {
          v->sound = -1;
          break;
        }
        v1715 = v->lvol >> 9;
        buf[s * 2] += d[v->position] * v1715 >> 7;
        v1715 = v->rvol >> 9;
        buf[s * 2 + 1] += d[v->position] * v1715 >> 7;
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
      for (s = 0; s < frames; ++s)
      {
        int v1715;
        float mod = sin(v->position * ff) * fm;
        int w = sin((v->position + mod) * ff) * 32767.0f;
        v1715 = v->lvol >> 9;
        buf[s * 2] += w * v1715 >> 7;
        v1715 = v->rvol >> 9;
        buf[s * 2 + 1] += w * v1715 >> 7;
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
static void sm_convert(Sint32 *input, int16 *output, int frames)
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


static void sm_callback(void *ud, Uint8 *stream, int len)
{
  /* 2 channels, 2 bytes/sample = 4 bytes/frame */
  len /= 4;
  while (len)
  {
    /* Audio processing */
    int frames = next_tick;
    if (frames > SM_MAXFRAGMENT)
      frames = SM_MAXFRAGMENT;
    if (frames > len) {
      frames = len;
    }
    sm_mixer(mixbuf, frames);
    if (audio_callback)
      audio_callback(mixbuf, frames);
    sm_convert(mixbuf, (int16 *)stream, frames);
    stream += frames * sizeof(int16) * 2;
    len -= frames;

    /* Control processing */
    next_tick -= frames;
    if (!next_tick)
    {
      if (subdivision_callback) {
        subdivision_callback(subdivision_payload);
      }
      if (control_callback) {
        interval = next_tick = control_callback(control_payload);
      }
      else {
        interval = next_tick = 10000;
      }
    }
  }
}

int sm_addvoice() {
  voices.resize(voices.size() + 1);
  return voices.size() - 1;
}

void sm_capvoices(int numVoices) {
  if (voices.size() > numVoices) {
    voices.resize(numVoices);
  }
}

int sm_open(int buffer, int numVoices)
{
  SDL_AudioSpec as = { 0 };
  int i;

  voices.resize(numVoices);

  mixbuf = (Sint32*)malloc(SM_MAXFRAGMENT * sizeof(Sint32) * 2);
  if (!mixbuf)
  {
    fprintf(stderr, "Couldn't allocate mixing buffer!\n");
    return -1;
  }

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
  {
    fprintf(stderr, "Couldn't init SDL audio: %s\n",
      SDL_GetError());
    return -2;
  }

  as.freq = 44100;
  as.format = AUDIO_S16SYS;
  as.channels = 2;
  as.samples = buffer;
  as.callback = sm_callback;

  audioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &as, &audiospec, 0);
  if (audioDeviceId == 0)
  {
    fprintf(stderr, "Couldn't open SDL audio: %s\n",
      SDL_GetError());
    return -3;
  }
  if (audiospec.format != AUDIO_S16SYS)
  {
    fprintf(stderr, "Wrong audio format!");
    return -4;
  }
  SDL_PauseAudioDevice(audioDeviceId, 0);
  return 0;
}


void sm_close(void)
{
  if (audioDeviceId != 0) {
    SDL_CloseAudioDevice(audioDeviceId);
    audioDeviceId = 0;
  }
  for (auto& sound : sounds) {
    sound.Unload();
  }
  sounds.clear();
  voices.clear();
  free(mixbuf);
}


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

int sm_load(const char *file) {
  int index = sounds.size();
  sm_load(index, file);
  return index;
}

int sm_load(int sound, const char *file)
{
  if (sound >= sounds.size()) {
    sounds.resize(sound + 1);
  }

  int failed = 0;
  SDL_AudioSpec spec;
  SDL_LockAudio();
  if (SDL_LoadWAV(file, &spec, &sounds[sound].data,
    &sounds[sound].length) == NULL)
  {
    SDL_UnlockAudio();
    return -1;
  }
  if (spec.freq != 44100)
    fprintf(stderr, "WARNING: File '%s' is not 44.1 kHz."
      " Might sound weird...\n", file);
  if (spec.channels != 1)
  {
    fprintf(stderr, "Only mono sounds are supported!\n");
    failed = 1;
  }
  switch (spec.format)
  {
  case AUDIO_S16LSB:
  case AUDIO_S16MSB:
    if (spec.format != AUDIO_S16SYS)
      flip_endian(sounds[sound].data, sounds[sound].length);
    break;
  default:
    fprintf(stderr, "Unsupported sample format!\n");
    failed = 1;
    break;
  }
  if (failed)
  {
    SDL_FreeWAV(sounds[sound].data);
    sounds[sound].data = NULL;
    sounds[sound].length = 0;
    SDL_UnlockAudio();
    return -2;
  }
  sounds[sound].filename = file;
  sounds[sound].channels = 1;
  sounds[sound].readbuf = sounds[sound].data;

  // Skip any inaudible intro ...
  // TODO: this should really be a pre-processing step
  auto frameSize = sizeof(uint16) * sounds[sound].channels;
  auto audibleOffset = sounds[sound].readbuf;
  auto audibleLength = static_cast<int32>(sounds[sound].length);
  while (audibleLength > 0) {
    int32 sum = 0;
    for (int c = 0; c < sounds[sound].channels; ++c) {
      sum += reinterpret_cast<int16*>(audibleOffset)[c];
    }

    if ((static_cast<float>(sum) / SHRT_MAX) > kSilenceThresholdIntro) {
      break;
    }

    audibleOffset += frameSize;
    audibleLength -= frameSize;
  }

  if (audibleLength > 0) {
    /*
    // Trimming the end is much more complicated ...
    // Cut any inaudible outro
    while (audibleLength > frameSize) {
      int32 sum = 0;
      for (int c = 0; c < sounds[sound].channels; ++c) {
        sum += reinterpret_cast<int16*>(audibleOffset + audibleLength - frameSize - 1)[c];
      }

      if ((static_cast<float>(sum) / SHRT_MAX) > kSilenceThresholdOutro) {
        break;
      }

      audibleLength -= frameSize;
    }
    */
    //assert(audibleLength > 0);
    sounds[sound].readbuf = audibleOffset;
    sounds[sound].length = audibleLength;
  }
  else {
    printf("Sound %s is entirely inaudible ... not trimming", file);
  }

  // Length is in bytes, we're counting 16-bit samples
  sounds[sound].length /= 2;
  SDL_UnlockAudio();
  return 0;
}


int sm_load_synth(int sound, const char *def)
{
  int res = 0;
  SDL_LockAudio();
  sounds[sound].data = (Uint8*)_strdup(def);
  if (strncmp(def, "fm2 ", 4) == 0)
  {
    if (sscanf(def, "fm2 %f %f %f",
      &sounds[sound].pitch,
      &sounds[sound].fm,
      &sounds[sound].decay) < 3)
    {
      fprintf(stderr, "fm2: Too few parameters!\n");
      res = -2;
    }
  }
  else
  {
    fprintf(stderr, "Unknown instrument type!\n");
    res = -1;
  }
  if (res < 0)
  {
    free(sounds[sound].data);
    sounds[sound].data = NULL;
    sounds[sound].length = 0;
  }
  SDL_UnlockAudio();
  return res;
}


void sm_set_control_cb(sm_control_cb cb, void* payload)
{
  SDL_LockAudio();
  control_callback = cb;
  control_payload = payload;
  next_tick = 0;
  SDL_UnlockAudio();
}


void sm_set_audio_cb(sm_audio_cb cb)
{
  SDL_LockAudio();
  audio_callback = cb;
  SDL_UnlockAudio();
}

void sm_set_subdivision_callback(sm_subdivision_callback cb, void* payload)
{
  subdivision_callback = cb;
  subdivision_payload = payload;
}

void sm_force_interval(unsigned interval)
{
  if (next_tick > interval)
    next_tick = interval;
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