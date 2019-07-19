#pragma once

#include "SDL.h"
#include "BaseTypes.h"
#include <string>
#include <vector>
#include <atomic>

class Patch;
class SoundInstance;
class ProcessInstance;
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
  std::vector<float> mixbuf;

  // A voice is a playing instance of a patch
  class Voice {
  public:
    Patch const* patch = nullptr;

    // Instances
    SoundInstance* sound = nullptr;
    ProcessInstance* process = nullptr;

    // Frame counter
    uint32 frame = 0;
  };
  std::vector<Voice> voices;

  // Refreshed every frame for thread-safe query
  std::atomic<uint32> numActiveVoices;

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

  bool Init(uint32 audioBufferSize);
  void PlayPatch(Patch const* patch, float volume);
  void ApplyInterval(uint32 interval);
  void SetController(Controller* controller);

   Mixer();
  ~Mixer();

  static bool InitSingleton(uint32 audioBufferSize);
  static bool TermSingleton();
  
  static inline Mixer& Get() {
    return *singleton;
  }
};