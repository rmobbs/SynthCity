#pragma once

#include "SDL.h"
#include "BaseTypes.h"
#include <string>
#include <map>
#include <vector>
#include <atomic>
#include "Sound.h"

class Mixer {
public:
  static constexpr uint32 kDefaultFrequency = 44100;
  static constexpr uint32 kDefaultChannels = 2;
  static constexpr float kDefaultMasterVolume = 0.7f;
protected:
  SDL_AudioSpec audiospec;
  int ticksPerFrame = 0;
  int ticksRemaining = 0;
  float masterVolume = kDefaultMasterVolume;
  SDL_AudioDeviceID audioDeviceId = 0;
  std::vector<float> mixbuf;

  // Refreshed every frame for thread-safe query
  std::atomic<uint32> numActiveVoices;

  void WriteOutput(float *input, int16 *output, int frames);

  // Used to control frame-based sequence
  virtual uint32 NextFrame();

public:
  SoundHandle nextSoundHandle = 0;
  void AudioCallback(void *ud, Uint8 *stream, int len);
  void MixVoices(float* mixBuffer, uint32 numFrames);
  std::vector<Voice*> voices;
  std::map<SoundHandle, Sound*> sounds;

  inline uint32 GetNumActiveVoices() const {
    return numActiveVoices;
  }

  inline float GetMasterVolume() const {
    return masterVolume;
  }
  void SetMasterVolume(float masterVolume);

  SoundHandle LoadSound(std::string fileName);
  SoundHandle AddSound(Sound* sound);
  void ReleaseSound(SoundHandle soundHandle);

  bool Init(uint32 audioBufferSize);
  void PlaySound(uint32 soundHandle, float volume);
  void ApplyInterval(uint32 interval);

  Sound* GetSound(SoundHandle soundHandle) const {
    // Gotta combine these classes dude
    auto sound = sounds.find(soundHandle);
    if (sound != sounds.end()) {
      return sound->second;
    }
    return nullptr;
  }

   Mixer();
  ~Mixer();
};