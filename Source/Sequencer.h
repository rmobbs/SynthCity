#pragma once

#include "BaseTypes.h"
#include "Mixer.h"
#include "SerializeFwd.h"

#include <vector>
#include <string>
#include <functional>

class Instrument;
class Patch;
class Song;

class Sequencer : public Mixer::Controller {
public:
  static constexpr uint32 kDefaultNumMeasures = 4;
  static constexpr uint32 kDfeaultBeatSubdivision = 4;
  static constexpr uint32 kMinTempo = 40;
  static constexpr uint32 kMaxTempo = 220;
  static constexpr uint32 kDefaultTempo = 120;
  static constexpr uint32 kDefaultInterval = 1000;

  // Loaded MIDI is passed to the host; they interact with the user to determine what
  // parameters will be used to convert that MIDI to our song format
  struct MidiConversionParams {
    uint32 tempo;
    bool rebaseToFirstNote = false;
    std::vector<uint32> trackIndices; // Tracks to flatten into single song
  };

  typedef void(*NotePlayedCallback)(uint32 trackIndex, uint32 noteIndex, void* payload);

private:
  static Sequencer* singleton;

  uint32 loopIndex = 0;
  uint32 currBeatSubdivision = kDfeaultBeatSubdivision;
  std::atomic<bool> isPlaying = false;
  std::atomic<bool> isMetrononeOn = false;
  std::atomic<bool> isLooping = true;
  std::vector<std::pair<NotePlayedCallback, void*>> notePlayedCallbacks;
  void* notePlayedPayload = nullptr;
  int32 currPosition = 0;
  int32 nextPosition = 0;
  int32 interval = kDefaultInterval;
  Instrument* instrument = nullptr;
  Song* song = nullptr;
  std::vector<Patch*> reservedPatches;
  std::function<bool(std::string)> loadInstrumentCallback;
  std::function<bool(const class MidiSource&, MidiConversionParams&)> midiConversionParamsCallback;

  void FullNoteCallback(bool isMeasure);
  uint32 UpdateInterval();


  // Mixer
  uint32 NextFrame() override;

public:
  inline bool IsMetronomeOn() const {
    return isMetrononeOn;
  }

  inline Instrument* GetInstrument() {
    return instrument;
  }

  inline Song* GetSong() const {
    return song;
  }

  inline void EnableMetronome(bool enabled) {
    isMetrononeOn = enabled;
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

  uint32 GetTempo() const;
  void SetTempo(uint32 bpm);

  inline bool IsPlaying() const {
    return isPlaying;
  }

  inline bool IsLooping() const {
    return isLooping;
  }

  void SetLooping(bool looping);

  void Play();
  void Pause();
  void PauseKill();
  void Stop();
  bool LoadInstrument(std::string fileName, std::string mustMatch);

  void NewSong();
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

  uint32 GetPosition() const;
  uint32 GetNextPosition() const;

  bool Init();

  void SetPosition(int32 newPosition);
  uint32 AddNotePlayedCallback(NotePlayedCallback notePlayedCallback, void* notePlayedPayload);
  void RemoveNotePlayedCallback(uint32 callbackId);
  bool NewInstrument();

   Sequencer() {}
  ~Sequencer();

  static bool InitSingleton();
  static bool TermSingleton();


  static inline Sequencer& Get() {
    return *singleton;
  }
};

