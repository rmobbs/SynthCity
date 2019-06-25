#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"
#include <string>
#include <vector>

class Sound;

class Track {
public:
  std::string name;
  std::string colorScheme;
  std::vector<uint8> data;
  int	skip = 0;
  int	mute = 0;
  int soundIndex = -1;
  int voiceIndex = -1;
  float	decay = 0.0f;
  float	lvol = 1.0f;
  float	rvol = 1.0f;

  Track();
  Track(Track&& other) noexcept; // Necessary for vector.resize ...
  ~Track();

  void AddNotes(uint32 noteCount, uint8 noteValue = 0);
  void SetNoteCount(uint32 noteCount, uint8 noteValue = 0);

  inline const std::string& GetColorScheme(void) const {
    return colorScheme;
  }
  inline const std::string& GetName(void) const {
    return name;
  }
  inline const std::vector<uint8>& GetNotes(void) const {
    return data;
  }
};

class Instrument {
public:
  static constexpr uint32 kNoteVelocityAsUint8 = 255;

  std::vector<Track> tracks;
  std::string name;
  uint32 numNotes;

  void AddTrack(std::string voiceName, std::string colorScheme, std::string fileName);
  void AddTrack(std::string voiceName, std::string colorScheme, Sound* synthSound);
  void ClearNotes();
  void Clear();
  void PlayTrack(uint32 trackIndex, float velocity);
  void SetNoteCount(uint32 numNotes);
  void SetTrackNote(uint32 trackIndex, uint32 noteIndex, float velocity);
  bool SaveInstrument(std::string fileName);

  Instrument(const ReadSerializer& r, uint32 numNotes);
  Instrument(std::string instrumentName, uint32 numNotes);
  ~Instrument();

  std::string GetName(void) const {
    return name;
  }
  void SetName(const std::string& name);

  inline const std::vector<Track>& GetTracks(void) const {
    return tracks;
  }

  static Instrument* LoadInstrument(std::string fileName, uint32 numNotes);

  bool SerializeRead(const ReadSerializer& serializer);
  bool SerializeWrite(const WriteSerializer& serializer);
};

