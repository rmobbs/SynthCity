#pragma once

#include "BaseTypes.h"
#include <vector>
#include <string>
#include <functional>
#include "SerializeFwd.h"

class Sound;
class Instrument;

class Sequencer {
private:
  static constexpr uint32 kDefaultBeatsPerMeasure = 4;
  static constexpr uint32 kDefaultNumMeasures = 4;
  static constexpr uint32 kMinTempo = 40;
  static constexpr uint32 kMaxTempo = 220;
  static constexpr float kDefaultMasterVolume = 0.7f;
public:
public:
  // Loaded MIDI is passed to the host; they interact with the user to determine what
  // parameters will be used to convert that MIDI to our song format
  struct MidiConversionParams {
    uint32 tempo;
    bool rebaseToFirstNote = false;
    std::vector<uint32> trackIndices; // Tracks to flatten into single song
  };

  typedef void(*NotePlayedCallback)(int trackIndex, int noteIndex, void* payload);

private:
  uint32 beatsPerMeasure = kDefaultBeatsPerMeasure;
  uint32 numMeasures = kDefaultNumMeasures;
  uint32 maxBeatSubdivisions;
  uint32 currBeatSubdivision;
  uint32 currentBpm = 120;
  float masterVolume = kDefaultMasterVolume;
  bool isPlaying = false;
  bool isMetrononeOn = false;
  bool isLooping = true;
  NotePlayedCallback notePlayedCallback = nullptr;
  void* notePlayedPayload = nullptr;
  int currPosition;
  int nextPosition;
  int interval;
  Instrument* instrument = nullptr;
  std::vector<uint32> reservedSounds;
  std::function<bool(std::string)> loadInstrumentCallback;
  std::function<bool(const class MidiSource&, MidiConversionParams&)> midiConversionParamsCallback;

  void PartialNoteCallback();
  void FullNoteCallback(bool isMeasure);

  uint32 CalcInterval(uint32 beatSubdivision) const;

public:
  inline bool IsMetronomeOn(void) const {
    return isMetrononeOn;
  }

  inline Instrument* GetInstrument(void) {
    return instrument;
  }

  inline void EnableMetronome(bool enabled) {
    isMetrononeOn = enabled;
  }

  inline uint32 GetNumMeasures() const {
    return numMeasures;
  }
  void SetNumMeasures(uint32 numMeasures);

  inline uint32 GetBeatsPerMeasure() const {
    return beatsPerMeasure;
  }
  void SetBeatsPerMeasure(uint32 beatsPerMeasure);

  inline uint32 GetSubdivision() const {
    return currBeatSubdivision;
  }

  void SetSubdivision(uint32 subdivision);

  inline uint32 GetMaxSubdivisions() const {
    return maxBeatSubdivisions;
  }

  inline uint32 GetMinTempo() const {
    return kMinTempo;
  }

  inline uint32 GetMaxTempo() const {
    return kMaxTempo;
  }

  inline uint32 GetBeatsPerMinute() const {
    return currentBpm;
  }
  void SetBeatsPerMinute(uint32 bpm);

  inline bool IsPlaying() const {
    return isPlaying;
  }

  inline bool IsLooping() const {
    return isLooping;
  }

  void SetLooping(bool looping);

  inline float GetMasterVolume() const {
    return masterVolume;
  }
  void SetMasterVolume(float masterVolume);

  void Play();
  void Pause();
  void Stop();
  bool LoadInstrument(std::string fileName, std::string mustMatch);

  bool SaveSong(std::string fileName);
  void LoadSongJson(std::string fileName);
  void LoadSongMidi(std::string fileName);
  void LoadSong(std::string fileName);

  inline void SetLoadInstrumentCallback(std::function<bool(std::string)> loadInstrumentCallback) {
    this->loadInstrumentCallback = loadInstrumentCallback;
  }

  inline void SetMidiConversionParamsCallback(std::function<bool(const class MidiSource&, MidiConversionParams&)> midiConversionParamsCallback) {
    this->midiConversionParamsCallback = midiConversionParamsCallback;
  }


  inline float GetMinutesPerBeat() const {
    return 1.0f / GetBeatsPerMinute();
  }

  inline float GetSecondsPerBeat() const {
    return GetMinutesPerBeat() * 60.0f;
  }

  uint32 GetPosition(void) const;
  uint32 GetNextPosition(void) const;

  bool Init(uint32 numMeasures, uint32 beatsPerMeasure, uint32 bpm, uint32 maxBeatSubdivisions, uint32 currBeatSubdivision);

  void SetPosition(uint32 newPosition);
  void SetNotePlayedCallback(NotePlayedCallback notePlayedCallback, void* notePlayedPayload);
  uint32 NextFrame(void);
  bool NewInstrument();

   Sequencer() {}
  ~Sequencer();

  static Sequencer& Get();
};

