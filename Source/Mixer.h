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
  
  // Used to control gameplay, editing, etc.
  class Controller {
  public:
    virtual uint32 NextFrame() = 0;

    virtual void OnDisconnect() {}
    virtual void OnConnect() {}
  };
protected:
  static Mixer* singleton;

  SDL_AudioSpec audioSpec = { 0 };
  SDL_AudioDeviceID audioDeviceId = 0;

  int32 ticksPerFrame = 0;
  int32 ticksRemaining = 0;
  float masterVolume = kDefaultMasterVolume;
  Controller* controller = nullptr;
  SoundHandle nextSoundHandle = 0;

  // Refreshed every frame for thread-safe query
  std::atomic<uint32> numActiveVoices;

  std::vector<float> mixbuf;
  std::vector<SoundInstance*> voices;
  std::map<SoundHandle, Sound*> sounds;

  void WriteOutput(float *input, int16 *output, int32 frames);

public:
  void AudioCallback(void *userData, uint8 *stream, int32 length);
  void MixVoices(float* mixBuffer, uint32 numFrames);

  inline uint32 GetNumActiveVoices() const {
    return numActiveVoices;
  }

  inline float GetMasterVolume() const {
    return masterVolume;
  }
  void SetMasterVolume(float masterVolume);

  SoundHandle AddSound(Sound* sound);
  void ReleaseSound(SoundHandle soundHandle);

  bool Init(uint32 audioBufferSize);
  void PlaySound(uint32 soundHandle, float volume);
  void ApplyInterval(uint32 interval);
  void SetController(Controller* controller);

  Sound* GetSound(SoundHandle soundHandle) const {
    auto sound = sounds.find(soundHandle);
    if (sound != sounds.end()) {
      return sound->second;
    }
    return nullptr;
  }

   Mixer();
  ~Mixer();

  static bool InitSingleton(uint32 audioBufferSize);
  static bool TermSingleton();
  
  static inline Mixer& Get() {
    return *singleton;
  }
};