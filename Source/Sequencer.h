#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include "Globals.h"

#include <vector>
#include <string>
#include <functional>
#include <map>
#include <array>
#include <atomic>
#include <queue>
#include <chrono>

class Patch;
class Voice;
class InputState;

class Sequencer {
public:
  static constexpr uint32 kMinTempo = 40;
  static constexpr uint32 kMaxTempo = 220;
  static constexpr uint32 kDefaultTempo = 120;
  static constexpr uint32 kDefaultInterval = 1000;
  static constexpr float kDefaultMasterVolume = 0.7f;

  class Listener {
    public:
      virtual void OnBeat(uint32 beatIndex) {

      }
  };

  // Loaded MIDI is passed to the host; they interact with the user to determine what
  // parameters will be used to convert that MIDI to our song format
  struct MidiConversionParams {
    uint32 tempo;
    bool rebaseToFirstNote = false;
    std::vector<uint32> trackIndices; // Tracks to flatten into single song
  };

private:
  static Sequencer* singleton;

  uint32 currBeat = 0;
  uint32 nextBeat = 0;
  int32 interval = kDefaultInterval;
  Listener* listener = nullptr;
  std::vector<Patch*> reservedPatches;
  std::function<bool(const class MidiSource&, MidiConversionParams&)> midiConversionParamsCallback;
  int32 ticksPerFrame = 0;
  int32 ticksRemaining = 0;
  std::vector<float> mixbuf;
  std::list<Voice*> voices;
  std::map<int32, Voice*> voiceMap;

  std::atomic<bool> isPlaying = false;
  std::atomic<float> masterVolume = kDefaultMasterVolume;
  std::atomic<uint32> numActiveVoices = 0;
  std::chrono::high_resolution_clock::time_point clockTimePoint;
  std::chrono::high_resolution_clock::time_point frameTimePoint;
  double frameBeatTime = 0.0;
  double clockBeatTime = 0.0;
  uint32 tempo = kDefaultTempo;

  void WriteOutput(float *input, int16 *output, int32 frames);
  void DrainExpiredPool();
  uint32 NextFrame();
  void AudioCallback(uint8 *stream, int32 length);
  void MixVoices(float* mixBuffer, uint32 numFrames);
  void StopAllVoices();

public:

  void SetTempo(uint32 newTempo);

  inline uint32 GetTempo() const {
    return tempo;
  }

  inline bool IsPlaying() const {
    return isPlaying;
  }

  inline uint32 GetNumActiveVoices() const {
    return numActiveVoices;
  }

  inline float GetMasterVolume() const {
    return masterVolume;
  }

  void SetMasterVolume(float masterVolume) {
    this->masterVolume = masterVolume;
  }

  double GetClockBeatTime();
  double GetFrameBeatTime();

  uint32 GetFrequency() const;

  void Play();
  void Pause();
  void PauseKill();
  void Stop();
  void StopKill();
  void Loop();
  void StopVoice(int32 voiceId);
  int32 PlayPatch(const Patch* patch, float volume);

  void PlayMetronome(bool downBeat);

  inline void SetMidiConversionParamsCallback(std::function<bool(const class MidiSource&, MidiConversionParams&)> midiConversionParamsCallback) {
    this->midiConversionParamsCallback = midiConversionParamsCallback;
  }

  uint32 GetPosition() const;
  uint32 GetNextPosition() const;

  bool Init();

  void SetPosition(uint32 newPosition);

   Sequencer();
  ~Sequencer();

  void SetListener(Listener* newListener);

  static bool InitSingleton();
  static bool TermSingleton();

  static inline Sequencer& Get() {
    return *singleton;
  }
};

