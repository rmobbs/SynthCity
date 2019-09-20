#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"

// NO NO NO
#include "GameInput.h"

#include <vector>
#include <string>
#include <functional>
#include <map>
#include <array>
#include <atomic>
#include <queue>

class Patch;
class Song;
class Voice;
class InputState;

class Sequencer {
public:
  static constexpr uint32 kDfeaultBeatSubdivision = 4;
  static constexpr uint32 kMinTempo = 40;
  static constexpr uint32 kMaxTempo = 220;
  static constexpr uint32 kDefaultTempo = 120;
  static constexpr uint32 kDefaultInterval = 1000;
  static constexpr float kDefaultMasterVolume = 0.7f;

  // Loaded MIDI is passed to the host; they interact with the user to determine what
  // parameters will be used to convert that MIDI to our song format
  struct MidiConversionParams {
    uint32 tempo;
    bool rebaseToFirstNote = false;
    std::vector<uint32> trackIndices; // Tracks to flatten into single song
  };

private:
  static Sequencer* singleton;

  GameInput gameInput;

  uint32 currBeatSubdivision = kDfeaultBeatSubdivision;
  void* notePlayedPayload = nullptr;
  uint32 currFrame = 0;
  uint32 nextFrame = 0;
  uint32 currBeat = 0;
  uint32 nextBeat = 0;
  int32 interval = kDefaultInterval;
  Song* song = nullptr;
  std::vector<Patch*> reservedPatches;
  std::function<bool(const class MidiSource&, MidiConversionParams&)> midiConversionParamsCallback;
  int32 ticksPerFrame = 0;
  int32 ticksRemaining = 0;
  uint32 songStartFrame = 0;
  uint32 introBeatCount = 0;
  std::vector<float> mixbuf;
  std::list<Voice*> voices;
  std::map<int32, Voice*> voiceMap;

  std::atomic<bool> isGameplayMode = false;
  std::atomic<bool> isPlaying = false;
  std::atomic<float> masterVolume = kDefaultMasterVolume;
  std::atomic<uint32> numActiveVoices = 0;
  std::atomic<float> beatTime = 0.0f;
  std::atomic<bool> isLooping = false;

  void WriteOutput(float *input, int16 *output, int32 frames);
  void DrainExpiredPool();
  uint32 NextFrame();
  void AudioCallback(void *userData, uint8 *stream, int32 length);
  void MixVoices(float* mixBuffer, uint32 numFrames);
  void StopAllVoices();

public:
  inline Song* GetSong() const {
    return song;
  }

  inline GameInput& GetGameInput() {
    return gameInput;
  }

  // Expects full beats
  void SetIntroBeats(uint32 introBeatCount);
  inline uint32 GetIntroBeats() const {
    return introBeatCount;
  }

  inline uint32 GetSubdivision() const {
    return currBeatSubdivision;
  }

  void SetSubdivision(uint32 subdivision);

  inline uint32 GetMinTempo() const {
    return kMinTempo;
  }

  inline uint32 GetMaxTempo() const {
    return kMaxTempo;
  }

  inline bool IsPlaying() const {
    return isPlaying;
  }

  inline bool IsLooping() const {
    return isLooping;
  }
  inline void SetLooping(bool isLooping) {
    this->isLooping = isLooping;
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

  inline void SetGameplayMode(bool gameplayMode) {
    isGameplayMode = gameplayMode;
  }

  inline float GetBeatTime() const {
    return beatTime;
  }

  uint32 GetFrequency() const;

  void Play();
  void Pause();
  void PauseKill();
  void Stop();
  void StopKill();
  void StopVoice(int32 voiceId);
  int32 PlayPatch(const Patch* patch, float volume);
  uint32 UpdateInterval();

  void PlayMetronome(bool downBeat);

  inline void SetMidiConversionParamsCallback(std::function<bool(const class MidiSource&, MidiConversionParams&)> midiConversionParamsCallback) {
    this->midiConversionParamsCallback = midiConversionParamsCallback;
  }

  uint32 GetPosition() const;
  uint32 GetNextPosition() const;

  bool Init();

  void SetPosition(uint32 newPosition);
  void SetSong(Song* newSong);

   Sequencer();
  ~Sequencer();

  static bool InitSingleton();
  static bool TermSingleton();

  static inline Sequencer& Get() {
    return *singleton;
  }
};

