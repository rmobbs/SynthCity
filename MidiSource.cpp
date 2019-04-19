#include "MidiSource.h"

#include <fstream>
#include <array>
#include <vector>
#include <streambuf>
#include <map>
#include <filesystem>

#include <cassert>

std::map<uint8, MidiEvent::EventType> ByteSignatureToReservedEventType = {
  { 0xFF, MidiEvent::EventType::Meta },
  { 0xF0, MidiEvent::EventType::Sysex },
  { 0xF7, MidiEvent::EventType::Sysex },
};

std::map<uint8, MidiEvent::MetaType> ByteSignatureToMidiMetaType = {
  { 0x00, MidiEvent::MetaType::SequenceNumber },
  { 0x01, MidiEvent::MetaType::TextEvent },
  { 0x02, MidiEvent::MetaType::CopyrightNotice },
  { 0x03, MidiEvent::MetaType::SequenceOrTrackName },
  { 0x04, MidiEvent::MetaType::InstrumentName },
  { 0x05, MidiEvent::MetaType::Lyric },
  { 0x06, MidiEvent::MetaType::Marker },
  { 0x07, MidiEvent::MetaType::CuePoint },
  { 0x20, MidiEvent::MetaType::MidiChannelPrefix },
  { 0x2F, MidiEvent::MetaType::EndOfTrack },
  { 0x51, MidiEvent::MetaType::SetTempo },
  { 0x54, MidiEvent::MetaType::SmtpeOffset },
  { 0x58, MidiEvent::MetaType::TimeSignature },
  { 0x59, MidiEvent::MetaType::KeySignature },
  { 0x7F, MidiEvent::MetaType::SequencerSpecificMetaEvent },
};

typedef void(*MetaDebugPrintFunction)(uchar* data, uint32 dataLen);
std::map<MidiEvent::MetaType, MetaDebugPrintFunction> metaDebugPrintFunctions = {
  {
    MidiEvent::MetaType::SequenceNumber,
    [](uchar* data, uint32 dataLen) {
      // This is an optional event, which must occur only at the start of a track,
      // before any non-zero delta-time.
      // For Format 2 MIDI files, this is used to identify each track. If omitted,
      // the sequences are numbered sequentially in the order the tracks appear.
      // For Format 1 files, this event should occur on the first track only.
      // 00 02 ss ss
      std::cout << "SequenceNumber: " << std::to_string(static_cast<uint16>(data[0] << 8) | data[1]) << std::endl;
    } 
  },
  {
    MidiEvent::MetaType::TextEvent,
    [](uchar* data, uint32 dataLen) {
      // This event is used for annotating the track with arbitrary text.
      // 01 <len> <text>
      std::cout << "TextEvent: " << std::string(reinterpret_cast<char *>(data), dataLen) << std::endl;
    }
  },
  {
    MidiEvent::MetaType::CopyrightNotice,
    [](uchar* data, uint32 dataLen) {
      // This event is for a copyright notice in ascii text. 
      // 02 <len> <text>
      std::cout << "CopyrightNotice: " << std::string(reinterpret_cast<char *>(data), dataLen) << std::endl;
    }
  },
  {
    MidiEvent::MetaType::SequenceOrTrackName,
    [](uchar* data, uint32 dataLen) {
      // Name of the sequence or track.
      // 03 <len> <text>
      std::cout << "SequenceOrTrackName: " << std::string(reinterpret_cast<char *>(data), dataLen) << std::endl;
    }
  },
  {
    MidiEvent::MetaType::InstrumentName,
    [](uchar* data, uint32 dataLen) {
      // A description of the instrument(s) used on this track.
      // This can also be used to describe instruments on a particular MIDI Channel
      // within a track, by preceding this event with the meta-event MIDI Channel.
      // 04 <len> <text>
      std::cout << "InstrumentName: " << std::string(reinterpret_cast<char *>(data), dataLen) << std::endl;
    }
  },
  {
    MidiEvent::MetaType::Lyric,
    [](uchar* data, uint32 dataLen) {
      // Lyrics for the song.
      // Normally, each syllable will have it's own lyric-event, which occurs at the
      // time the lyric is to be sung. 
      // 05 <len> <text>
      std::cout << "Lyric: " << std::string(reinterpret_cast<char *>(data), dataLen) << std::endl;
    }
  },
  {
    MidiEvent::MetaType::Marker,
    [](uchar* data, uint32 dataLen) {
      // Normally on the first track of a format 1 or format 0 file.
      // Marks a significant point in the sequence(e.g. "Verse 1").
      // 06 <len> <text>
      std::cout << "Marker: " << std::string(reinterpret_cast<char *>(data), dataLen) << std::endl;
    }
  },
  {
    MidiEvent::MetaType::CuePoint,
    [](uchar* data, uint32 dataLen) {
      // Used to include cues for events happening on-stage.
      // 07 <len> <text>
      std::cout << "CuePoint: " << std::string(reinterpret_cast<char *>(data), dataLen) << std::endl;
    }
  },
  {
    MidiEvent::MetaType::MidiChannelPrefix,
    [](uchar* data, uint32 dataLen) {
      // Associate all following meta-events and sysex-events with the specified MIDI channel,
      // until the next <midi_event> (which must contain MIDI channel information).
      // 20 01 cc
      std::cout << "ChannelPrefix: " << std::to_string(data[0]) << std::endl;
    }
  },
  {
    MidiEvent::MetaType::EndOfTrack,
    [](uchar* data, uint32 dataLen) {
      // This event is required. It is used to give the track a clearly defined length, which is
      // essential information if the track is looped or concatenated with another track.
      // 2F 00
      std::cout << "EndOfTrack" << std::endl;
    }
  },
  {
    MidiEvent::MetaType::SetTempo,
    [](uchar* data, uint32 dataLen) {
      // This sets the tempo in microseconds per quarter note. This means a change in the unit-
      // length of a delta-time tick.
      // 51 03 tt tt tt
      double tempo;
      uint64 beatLengthInUs = static_cast<uint64>
        ((data[0] << 16) | (data[1] << 8) | (data[2]));
      tempo = (1000000.0 / static_cast<double>(beatLengthInUs)) * 60.0;
      std::cout << "SetTempo: " << std::to_string(tempo) << std::endl;
    }
  },
  {
    MidiEvent::MetaType::SmtpeOffset,
    [](uchar* data, uint32 dataLen) {
      // This (optional) event specifies the SMTPE time at which the track is to start. This event
      // must occur before any non-zero delta times, and before any MIDI events.
      // 54 05 hh mm ss fr ff
      std::cout << "SmtpeOffset: " << 
        std::to_string(data[0]) << "h" << 
        std::to_string(data[1]) << "m" << 
        std::to_string(data[2]) << "s" << 
        std::to_string(data[3]) << "fr" << 
        std::to_string(data[4]) << "ff" << std::endl;
    }
  },
  {
    MidiEvent::MetaType::TimeSignature,
    [](uchar* data, uint32 dataLen) {
      // Time signature of the form:
      // nn / 2 ^ dd
      // eg: 6 / 8 would be specified using nn = 6, dd = 3
      // The parameter cc is the number of MIDI Clocks per metronome tick.
      // Normally, there are 24 MIDI Clocks per quarter note.However, some software allows this to be
      // set by the user.The parameter bb defines this in terms of the number of 1/32 notes which make
      // up the usual 24 MIDI Clocks(the 'standard' quarter note).
      // 58 04 nn dd cc bb
      std::cout << "TimeSignature: " <<
        std::to_string(data[0]) << "/" <<
        std::to_string(static_cast<uint16>(powl(2, data[1]))) <<
        " " <<
        std::to_string(data[2]) <<
        "|" <<
        std::to_string(data[3]) << std::endl;
    }
  },
  {
    MidiEvent::MetaType::KeySignature,
    [](uchar* data, uint32 dataLen) {
      // Key Signature, expressed as the number of sharps or flats, and a major/minor flag.
      // 0 represents a key of C. Negative numbers represent flats. Positive numbers represent sharps.
      // 59 02 sf mi
      std::cout << "KeySignature: ";

      // sf is actually signed
      char c = static_cast<char>(data[0]);
      if (c < 0) {
        std::cout << std::to_string(std::abs(c)) << " flats";
      }
      else if (c > 0) {
        std::cout << std::to_string(c) << " sharps";
      }
      else {
        std::cout << "C ";
      }

      if (data[0]) {
        std::cout << " minor";
      }
      else {
        std::cout << " major";
      }

      std::cout << std::endl;
    }
  },
  {
    MidiEvent::MetaType::SequencerSpecificMetaEvent,
    [](uchar* data, uint32 dataLen) {
      // This is the MIDI-file equivalent of the System Exclusive Message. A manufacturer may incorporate
      // sequencer-specific directives into a MIDI file using this event.
      // Because ID can be 1 or 3 bytes and there is apparently no way to determine its length, this
      // is a rather uninformative debug message, but we don't handle this anyway.
      // 7F <len> <id> <data>
      std::cout << "SequencerSpecific " << std::to_string(data[0]) << std::endl;
    }
  },
};

MidiSource::MidiSource() {

}

bool MidiSource::openFile(const std::string& fileName) {
  std::ifstream ifs(fileName, std::ios::binary);
  if (!ifs) {
    std::cerr << "Unable to open MIDI file " << fileName << std::endl;
    return false;
  }

  // Read the file into a buffer
  endian_bytestream ebs;
  ebs << ifs.rdbuf();
  ifs.close();

  // Header
  if (!parseHeader(ebs)) {
    std::cerr << "Unable to parse MIDI file header" << std::endl;
    return false;
  }

  // Tracks
  for (uint32 trackIndex = 0; trackIndex < static_cast<uint32>(this->getTrackCount()); ++trackIndex) {
    if (!readTrack(ebs, trackIndex)) {
      return false;
    }
  }

  // Debug info
  std::cout << "MIDI file " << std::filesystem::path(fileName).filename().string() << std::endl;
  std::cout << "  Version:" << std::to_string(formatType) 
    << "  Tracks:" << std::to_string(tracks.size()) << std::endl;

  for (const auto& track : this->tracks) {
    auto channelIter = track.channels.begin();

    // Print basic info
    std::cout << "  Track " << std::to_string(track.index)
      << " meta:" << std::to_string(track.metaCount)
      << " msg:" << std::to_string(track.messageCount);

    if (track.channels.size() > 0) {
      std::cout << " channels: " << "{" << std::to_string(*channelIter);
      for (++channelIter; channelIter != track.channels.end(); ++channelIter) {
        std::cout << "," << std::to_string(*channelIter);
      }
      std::cout << "}";
    }
    std::cout << std::endl;

    // Print meta events
    for (const auto& midiEvent : track.events) {
      if (midiEvent.eventType != MidiEvent::EventType::Meta) {
        continue;
      }

      const auto& debugPrintFunc = metaDebugPrintFunctions.find(midiEvent.meta.type);
      assert(debugPrintFunc != metaDebugPrintFunctions.end());
      //std::cout << "    t=" << std::to_string(midiEvent.timeStamp) << ": ";
      debugPrintFunc->second(midiEvent.dataptr, midiEvent.datalen);
    }
  }

  return true;
}

void MidiSource::close() {
  // future-proofing
}

bool MidiSource::parseChunk(endian_bytestream& ebs, const std::string& expectedChunkId) {
  char chunkId[4] = { };

  ebs.read(chunkId, 4);

  if (!ebs.isGood("while reading chunk")) {
    return false;
  }

  if (strncmp(chunkId, expectedChunkId.c_str(), 4) != 0) {
    std::cerr << "Unexpected chunk ID " <<
      std::string(chunkId, chunkId + 4) <<
      " (expected " << expectedChunkId << ")" << std::endl;
    return false;
  }

  return true;
}

bool MidiSource::parseHeader(endian_bytestream& ebs) {
  // MThd character tag
  if (!parseChunk(ebs, "MThd")) {
    std::cerr << "Unable to find MIDI file header tag" << std::endl;
    return false;
  }

  // Header byte count, uint32
  uint32 byteCount;
  ebs >> byteCount;
  if (!ebs.isGood("while reading header byte count")) {
    return false;
  }
  if (byteCount != 6) {
    std::cerr << "Unexpected header byte count of " << byteCount << " (expected 6)" << std::endl;
    return false;
  }

  // Format type
  ebs >> formatType;
  if (!ebs.isGood("while reading format type")) {
    return false;
  }

  // Number of tracks
  uint16 trackCount;
  ebs >> trackCount;
  if (!ebs.isGood("while reading track count")) {
    return false;
  }
  tracks.resize(trackCount);

  // Time division
  ebs >> timeDivision;
  if (!ebs.isGood("while reading time division")) {
    return false;
  }

  // If MSB is set, it's SMTPE frame time
  if (timeDivision & 0x8000) {
    timeDivisionType = TimeDivisionType::SmpteFrameData;

    // Currently not supported
    std::cerr << "SMTPE frame based time division is currently not supported" << std::endl;
    return false;
  }
  else {
    timeDivisionType = TimeDivisionType::TicksPerQuarterNote;
  }

  return true;
}

void MidiSource::setNativeTempo(uint32 nativeTempo) {
  this->nativeTempo = nativeTempo;
}

bool MidiSource::readTrack(endian_bytestream& ebs, uint32 trackIndex) {
  assert(trackIndex < tracks.size());

  MidiTrack& currentTrack = tracks[trackIndex];

  currentTrack.index = trackIndex;

  // MTrk character tag
  if (!parseChunk(ebs, "MTrk")) {
    std::cerr << "Expected to find MTrk tag at start of track" << std::endl;
    return false;
  }

  uint32 byteCount;
  ebs >> byteCount;
  if (!ebs.isGood("while reading track byte count")) {
    return false;
  }

  // To reduce fragmentation, all event data is stored in a contiguous block;
  // as this can resize several times we will store indices as we read events,
  // then fixup pointers
  std::vector<uint32> eventDataIndex;

  uint32 lastByte = static_cast<uint32>(ebs.tellg()) + byteCount;
  while (static_cast<uint32>(ebs.tellg()) < lastByte) {
    // First entry for each data element is a variable-length delta time stored
    // as a series of byte chunks.
    // If the MSB is set this byte contributes the next 7 bits to the delta time
    uint32 deltaTime = 0;
    uint8 readByte;
    do {
      ebs >> readByte;
      if (!ebs.isGood("while reading data event delta time")) {
        return false;
      }
      deltaTime = (deltaTime << 7) | (readByte & 0x7F);
    } while (readByte & 0x80);

    // Next is the event type
    ebs >> readByte;
    if (!ebs.isGood("while reading event type")) {
      return false;
    }

    // Check for reserved event types
    const auto eventType = ByteSignatureToReservedEventType.find(readByte);
    if (eventType != ByteSignatureToReservedEventType.end()) {
      MidiEvent currentEvent;

      currentEvent.timeDelta = deltaTime;
      currentEvent.eventType = eventType->second;

      switch (eventType->second) {
        case MidiEvent::EventType::Meta: {
          // Meta type
          ebs >> readByte;
          if (!ebs.isGood("while reading meta type")) {
            return false;
          }

          // Grab the actual meta type, to determine if we store or skip
          const auto metaType = ByteSignatureToMidiMetaType.find(readByte);

          // Data size
          ebs >> readByte;
          if (!ebs.isGood("while reading meta data size")) {
            return false;
          }

          // Only store recognized/requested types
          if (metaType != ByteSignatureToMidiMetaType.end()) {
            ++currentTrack.metaCount;

            currentEvent.meta.type = metaType->second;

            if (readByte > 0) {
              // Data
              currentEvent.datalen = readByte;
              eventDataIndex.push_back(currentTrack.eventData.size());
              currentTrack.eventData.resize(currentTrack.eventData.size() + currentEvent.datalen);
              ebs.read(reinterpret_cast<char *>(currentTrack.
                eventData.data() + eventDataIndex.back()), currentEvent.datalen);
              if (!ebs.isGood("while reading meta data")) {
                return false;
              }
            }
            else {
              // Push empty marker into data index vector for bookkeeping
              eventDataIndex.push_back(-1);
            }

            currentTrack.events.push_back(currentEvent);

            if (currentEvent.meta.type == MidiEvent::MetaType::SetTempo) {
              uint8* data = currentTrack.eventData.data() + eventDataIndex.back();

              double tempo;
              uint64 beatLengthInUs = static_cast<uint64>
                ((data[0] << 16) | (data[1] << 8) | (data[2]));
              tempo = (1000000.0 / static_cast<double>(beatLengthInUs)) * 60.0;

              setNativeTempo(static_cast<uint32>(tempo));
            }
          }
          // Otherwise just skip it
          else if (readByte > 0) {
            ebs.seekg(readByte, std::ios_base::cur);
            if (!ebs.isGood("while skipping meta data")) {
              return false;
            }
          }
          break;
        }
        case MidiEvent::EventType::Sysex: {
          // Data size
          ebs >> readByte;
          if (!ebs.isGood("while reading sysex data size")) {
            return false;
          }

          // Just skip it
          ebs.seekg(readByte, std::ios_base::cur);
          if (!ebs.isGood("while skipping sysex data")) {
            return false;
          }
          break;
        }
      }
    }
    // All other event types are messages
    else {
      MidiEvent currentEvent;

      // Still unsure what a channel is; some documentation says it's the instrument,
      // but instruments seem to be associated with tracks
      currentTrack.channels.insert(readByte & 0x0F);

      currentEvent.timeDelta = deltaTime;
      currentEvent.eventType = MidiEvent::EventType::Message;

      // All messages have a byte of status plus at least one byte of data
      uchar dataByte;
      ebs >> dataByte;
      if (!ebs.isGood("while reading message data 0")) {
        return false;
      }

      // Determine the message type
      if ((readByte & 0xF0) == 0x80) {
        currentEvent.message.type = MidiEvent::MessageType::VoiceNoteOff;
      }
      else if ((readByte & 0xF0) == 0x90) {
        currentEvent.message.type = MidiEvent::MessageType::VoiceNoteOn;
      }
      else if ((readByte & 0xF0) == 0xA0) {
        currentEvent.message.type = MidiEvent::MessageType::VoicePolyphonicKeyPressure;
      }
      else if ((readByte & 0xF0) == 0xB0) {
        switch (dataByte) {
          case 0x78:
            currentEvent.message.type = MidiEvent::MessageType::ModeAllSoundOff;
            break;
          case 0x79:
            currentEvent.message.type = MidiEvent::MessageType::ModeResetAllControllers;
            break;
          case 0x7A:
            currentEvent.message.type = MidiEvent::MessageType::ModeLocalControl;
            break;
          case 0x7B:
            currentEvent.message.type = MidiEvent::MessageType::ModeAllNotesOff;
            break;
          case 0x7C:
            currentEvent.message.type = MidiEvent::MessageType::ModeOmniModeOff;
            break;
          case 0x7D:
            currentEvent.message.type = MidiEvent::MessageType::ModeOmniModeOn;
            break;
          case 0x7E:
            currentEvent.message.type = MidiEvent::MessageType::ModePolyModeOn;
            break;
          default:
            currentEvent.message.type = MidiEvent::MessageType::VoiceControllerChange;
            break;
        }
      }
      else if ((readByte & 0xF0) == 0xC0) {
        currentEvent.message.type = MidiEvent::MessageType::VoiceProgramChange;
      }
      else if ((readByte & 0xF0) == 0xD0) {
        currentEvent.message.type = MidiEvent::MessageType::VoiceKeyPressure;
      }
      else if ((readByte & 0xF0) == 0xE0) {
        currentEvent.message.type = MidiEvent::MessageType::VoicePitchBend;
      }
      else {
        currentEvent.message.type = MidiEvent::MessageType::Unknown;
      }

      if (currentEvent.message.type == MidiEvent::MessageType::Unknown) {
        // If we don't know what it is, we can attempt to skip it; can't be worse
        // than just returning
        std::cerr << "Encountered unknown message type ... "
          "skipping 2 bytes of data but errors could result" << std::endl;
        ebs.seekg(1, std::ios_base::cur);
        if (!ebs.isGood("while skipping unknown message data")) {
          return false;
        }
      }
      else {
        ++currentTrack.messageCount;

        // Mark location in data buffer
        eventDataIndex.push_back(currentTrack.eventData.size());

        // Ensure we have space in the data buffer
        if (currentEvent.message.type == MidiEvent::MessageType::VoiceProgramChange ||
            currentEvent.message.type == MidiEvent::MessageType::VoiceKeyPressure) {
          currentEvent.datalen = 2;
        }
        else {
          currentEvent.datalen = 3;
        }
        currentTrack.eventData.resize(currentTrack.
          eventData.size() + currentEvent.datalen);

        // Store status byte
        currentTrack.eventData[eventDataIndex.back()] = readByte;

        // Store data byte 0
        currentTrack.eventData[eventDataIndex.back() + 1] = dataByte;
        if (currentEvent.datalen > 2) {
          // Store data byte 1
          ebs >> dataByte;
          currentTrack.eventData[eventDataIndex.back() + 2] = dataByte;
          if (!ebs.isGood("while reading message data 1")) {
            return false;
          }
        }

        currentTrack.events.push_back(currentEvent);
      }
    }
  }

  // Clean up memory
  currentTrack.eventData.shrink_to_fit();

  assert(eventDataIndex.size() == currentTrack.events.size());

  auto eventIter = currentTrack.events.begin();
  for (const auto& dataIndex : eventDataIndex) {
    // Fixup data pointers
    if (dataIndex != -1) {
      eventIter->dataptr = currentTrack.eventData.data() + dataIndex;
    }

    // TODO: We're assuming all meta events happen at t=0 anyway (don't know
    // how we'd handle a mid-stream tempo change) so meta events should be
    // separated and processed first.

    // Build playback sequence
    switch (eventIter->eventType) {
      case MidiEvent::EventType::Meta: {
        switch (eventIter->meta.type) {
          case MidiEvent::MetaType::SetTempo:
          case MidiEvent::MetaType::TimeSignature:
          case MidiEvent::MetaType::EndOfTrack:
            currentTrack.sequence.push(*eventIter);
            break;
        }
        break;
      }
      case MidiEvent::EventType::Message: {
        currentTrack.sequence.push(*eventIter);
        break;
      }
      default:
        break;
    }

    ++eventIter;
  }

  return true;
}

