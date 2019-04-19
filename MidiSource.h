#pragma once

#include <stdio.h>
#include <string>
#include <vector>
#include <queue>
#include "BaseTypes.h"
#include <sstream>
#include <iostream>
#include <set>

struct MidiEvent {
  enum class EventType {
    Meta,
    Message,
    Sysex,
  };

  enum class MetaType {
    SequenceNumber,
    TextEvent,
    CopyrightNotice,
    SequenceOrTrackName,
    InstrumentName,
    Lyric,
    Marker,
    CuePoint,
    MidiChannelPrefix,
    EndOfTrack,
    SetTempo,
    SmtpeOffset,
    TimeSignature,
    KeySignature,
    SequencerSpecificMetaEvent,
  };

  enum class MessageType {
    Unknown,
    VoiceNoteOff,
    VoiceNoteOn,
    VoicePolyphonicKeyPressure,
    VoiceControllerChange,
    VoiceProgramChange,
    VoiceKeyPressure,
    VoicePitchBend,
    ModeAllSoundOff,
    ModeResetAllControllers,
    ModeLocalControl,
    ModeAllNotesOff,
    ModeOmniModeOff,
    ModeOmniModeOn,
    ModeMonoModeOn,
    ModePolyModeOn,
  };

  EventType eventType;

  union {
    struct {
      MetaType type;
    } meta;
    struct {
      MessageType type;
    } message;
  }; 

  uint64 timeDelta = 0;

  uchar* dataptr = nullptr;
  uint16 datalen = 0;
};

struct MidiTrack {
  std::vector<MidiEvent> events;
  std::vector<uchar> eventData;
  std::queue<MidiEvent> sequence;
  std::set<uchar> channels;

  uint32 index;
  uint32 metaCount = 0;
  uint32 messageCount = 0;
};

// Stringstream's >> operator fails and marks EOF if you try to read a 0;
// this custom extension avoids that behavior and handles Endian swapping.
class endian_bytestream : public std::stringstream {
public:
  explicit endian_bytestream()
  {
  }

  template <typename T> inline endian_bytestream& operator>>(T& outData) {
    this->read(reinterpret_cast<char *>(&outData), sizeof(outData));

    // One little Endian
    for (int i = 0; i < sizeof(T) / 2; ++i) {
      std::swap(reinterpret_cast<char *>(&outData)[i],
        reinterpret_cast<char *>(&outData)[sizeof(T) - 1 - i]);
    }
    return *this;
  }

  bool isGood(const std::string& errorTag = { }) {
    if (eof()) {
      std::cerr << "Unexpected EOF " << errorTag << std::endl;
      return false;
    }
    if (fail()) {
      std::cerr << "Unknown failure " << errorTag << std::endl;
      return false;
    }
    return true;
  }
};

class MidiSource {
protected:
  static constexpr uint16 kDefaultTimeDivision = 96;
  static constexpr uint32 kDefaultNativeTempo = 120;

  enum class TimeDivisionType {
    Unknown,
    TicksPerQuarterNote,
    SmpteFrameData,
  };

  TimeDivisionType timeDivisionType = TimeDivisionType::Unknown;

  uint16 formatType = 0;
  uint16 timeDivision = kDefaultTimeDivision;
  uint32 nativeTempo = kDefaultNativeTempo;

  std::vector<MidiTrack> tracks;

  bool readTrack(endian_bytestream& ebs, uint32 trackIndex);
  bool parseHeader(endian_bytestream& ebs);
  bool parseChunk(endian_bytestream& ebs, const std::string& expectedChunkId);
  void setNativeTempo(uint32 nativeTempo);

public:
  bool openFile(const std::string& fileName);
  void close();

  inline const std::vector<MidiTrack> getTracks() {
    return tracks;
  }

  inline size_t getTrackCount() const {
    return tracks.size();
  }

  inline uint16 getFormatType() const {
    return formatType;
  }

  inline uint32 getNativeTempo() const {
    return nativeTempo;
  }

  MidiSource();
};
