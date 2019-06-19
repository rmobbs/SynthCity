#pragma once

#include "SDL.h"
#include "BaseTypes.h"
#include <string>
#include <map>
#include <vector>

typedef unsigned (*sm_control_cb)(void* payload);
void sm_set_control_cb(sm_control_cb cb, void* payload);

class Sound;

class Mixer {
public:
  using SoundHandle = uint32;
  static constexpr SoundHandle kInvalidSoundHandle = 0xFFFFFFFF;
  static constexpr uint32 kDefaultFrequency = 44100;
  static constexpr uint32 kDefaultChannels = 2;
protected:
  struct Voice
  {
    Mixer::SoundHandle sound = kInvalidSoundHandle;

    int	position = 0;
    // 8:24 fixed point
    int	lvol = 0;
    int	rvol = 0;
    // 16:16 fixed point
    int	decay = 0;
  };

  static Mixer* singleton;
  SDL_AudioSpec audiospec;
  int ticksPerFrame = 0;
  int ticksRemaining = 0;
  SDL_AudioDeviceID audioDeviceId = 0;
  std::vector<uint8> mixbuf;

  void WriteOutput(Sint32 *input, int16 *output, int frames);

public:
  SoundHandle nextSoundHandle = 0;
  void AudioCallback(void *ud, Uint8 *stream, int len);
  void MixVoices(int32* mixBuffer, uint32 numFrames);
  std::vector<Voice> voices;
  std::map<SoundHandle, Sound*> sounds;

  static Mixer& Get(void) {
    return *singleton;
  }

  inline uint32 GetNumActiveVoices() const {
    // THIS NEEDS TO BE THREAD SAFE
    return voices.size();
  }

  SoundHandle LoadSound(std::string fileName);
  SoundHandle AddSound(Sound* sound);
  void ReleaseSound(SoundHandle soundHandle);

  bool Init(uint32 audioBufferSize);
  void Play(uint32 soundHandle, float volume);
  void ApplyInterval(uint32 interval);

  static bool InitSingleton(uint32 audioBufferSize);
  static void TermSingleton();

   Mixer();
  ~Mixer();
};