#pragma once

#include "BaseTypes.h"
#include "Mixer.h"
#include "SerializeFwd.h"

#include <vector>
#include <string>
#include <functional>

class Sound;
class Instrument;

class Sequencer : public Mixer::Controller {
private:
  static constexpr uint32 kDefaultBeatsPerMeasure = 4;
  static constexpr uint32 kDefaultNumMeasures = 4;
  static constexpr uint32 kMinTempo = 40;
  static constexpr uint32 kMaxTempo = 220;
  static constexpr uint32 kDefaultTempo = 120;
public:
  // Loaded MIDI is passed to the host; they interact with the user to determine what
  // parameters will be used to convert that MIDI to our song format
  struct MidiConversionParams {
    uint32 tempo;
    bool rebaseToFirstNote = false;
    std::vector<uint32> trackIndices; // Tracks to flatten into single song
  };

  typedef void(*NotePlayedCallback)(int32 trackIndex, int32 noteIndex, void* payload);

private:
  static Sequencer* singleton;

  uint32 beatsPerMeasure = kDefaultBeatsPerMeasure;
  uint32 numMeasures = kDefaultNumMeasures;
  uint32 maxBeatSubdivisions = 0;
  uint32 currBeatSubdivision = 0;
  uint32 currentBpm = kDefaultTempo;
  bool isPlaying = false;
  bool isMetrononeOn = false;
  std::atomic<bool> isLooping = true;
  NotePlayedCallback notePlayedCallback = nullptr;
  void* notePlayedPayload = nullptr;
  int32 currPosition = 0;
  int32 nextPosition = 0;
  int32 interval = 0;
  Instrument* instrument = nullptr;
  std::vector<uint32> reservedSounds;
  std::function<bool(std::string)> loadInstrumentCallback;
  std::function<bool(const class MidiSource&, MidiConversionParams&)> midiConversionParamsCallback;

  void PartialNoteCallback();
  void FullNoteCallback(bool isMeasure);
  uint32 CalcInterval(uint32 beatSubdivision) const;


  // Mixer
  uint32 NextFrame() override;

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
  bool NewInstrument();

   Sequencer() {}
  ~Sequencer();

  static bool InitSingleton(uint32 numMeasures, uint32 beatsPerMeasure, uint32 bpm, uint32 maxBeatSubdivisions, uint32 currBeatSubdivision);
  static bool TermSingleton();


  static inline Sequencer& Get() {
    return *singleton;
  }
};

