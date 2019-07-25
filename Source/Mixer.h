#pragma once

#include "BaseTypes.h"
#include <string>
#include <vector>
#include <map>
#include <list>
#include <atomic>

class Voice;
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

  int32 ticksPerFrame = 0;
  int32 ticksRemaining = 0;
  float masterVolume = kDefaultMasterVolume;
  Controller* controller = nullptr;
  std::vector<float> mixbuf;

  std::list<Voice*> voices;
  std::map<int32, Voice*> voiceMap;

  // Refreshed every frame for thread-safe query
  std::atomic<uint32> numActiveVoices;

  void WriteOutput(float *input, int16 *output, int32 frames);
  void DrainExpiredPool();

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
  void StopAllVoices();
  void StopVoice(int32 voiceId);
  int32 PlayPatch(const Patch* patch, float volume);
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