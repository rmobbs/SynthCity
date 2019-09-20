#include "Song.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Globals.h"
#include "Instrument.h"
#include "OddsAndEnds.h"

#include <fstream>

static constexpr const char *kInstrumentTag = "Instrument";
static constexpr const char *kTempoTag = "Tempo";
static constexpr const char *kMeasuresTag = "Measures";
static constexpr const char *kTimeSignatureTag = "TimeSignature";
static constexpr const char* kMinimumNoteDurationTag = "MinimumNoteDuration";
static constexpr const char* kTracksTag = "Tracks";
static constexpr const char *kNotesTag = "Notes";
static constexpr const char *kBeatTag = "Beat";
static constexpr const char* kFretTag = "Fret";
static constexpr const char* kGameLineTag = "GameLine";
static constexpr const char* kTrackIdTag = "TrackId";
static constexpr uint32 kSongFileVersion = 2;
static constexpr uint32 kDefaultNoteValue = 4;

Song::Song(std::string name, uint32 tempo, uint32 numMeasures, uint32 beatsPerMeasure, uint32 minNoteValue)
  : name(name)
  , tempo(tempo)
  , numMeasures(numMeasures)
  , beatsPerMeasure(beatsPerMeasure)
  , minNoteValue(minNoteValue) {

}

Song::Song(const ReadSerializer& serializer, std::function<Instrument*(std::string)> instrumentLoader)
  : instrumentLoader(instrumentLoader) {
  auto result = SerializeRead(serializer);
  if (!result.first) {
    throw std::runtime_error("Failed to serialize (read): " + result.second);
  }
}

Song::~Song() {
  delete instrument;
  instrument = nullptr;
}

std::pair<bool, std::string> Song::SerializeRead(const ReadSerializer& serializer) {
  const auto& d = serializer.d;

  if (!d.IsObject()) {
    return std::make_pair(false, "Expected an object");
  }

  // Version
  if (!d.HasMember(Globals::kVersionTag) || !d[Globals::kVersionTag].IsUint()) {
    return std::make_pair(false, "Missing/invalid version tag");
  }

  auto version = d[Globals::kVersionTag].GetUint();

  switch (version) {
    case 1: {
      name = std::filesystem::path(serializer.fileName).stem().generic_string();

      // Single instrument by name
      if (!d.HasMember(kInstrumentTag) || !d[kInstrumentTag].IsString()) {
        return std::make_pair(false, "Missing instrument tag");
      }

      std::string instrumentName = d[kInstrumentTag].GetString();

      if (!instrumentLoader) {
        return std::make_pair(false, "No instrument loader callback provided to load instrument for song");
      }

      instrument = instrumentLoader(instrumentName);
      if (!instrument) {
        return std::make_pair(false, "Unable to load required instrument to load song");
      }
      MCLOG(Warn, "Song is version 1 and this will be deprecated. Please re-save.");
      break;
    }
    case 2: {
      // Name
      if (!d.HasMember(Globals::kNameTag) || !d[Globals::kNameTag].IsString()) {
        return std::make_pair(false, "Missing name tag");
      }

      name = d[Globals::kNameTag].GetString();
      break;
    }
    default:
      return std::make_pair(false, "Invalid song file version");
  }

  // Get tempo
  if (!d.HasMember(kTempoTag) || !d[kTempoTag].IsUint()) {
    return std::make_pair(false, "Missing/invalid tempo");
  }

  tempo = d[kTempoTag].GetUint();

  // Get time signature
  if (!d.HasMember(kTimeSignatureTag) || !d[kTimeSignatureTag].IsString()) {
    return std::make_pair(false, "Missing/invalid time signature");
  }

  std::string timeSignature = d[kTimeSignatureTag].GetString();

  auto div = timeSignature.find('/');
  if (div != std::string::npos) {
    int32 signedBeatsPerMeasure = std::stoi(timeSignature.substr(0, div));

    if (beatsPerMeasure <= 0) {
      return std::make_pair(false, "Beats per measure cannot be <= 0");
    }

    // We currently assume a quarter-note base note value
    // https://trello.com/c/WY2eBMDh
  }
  else {
    return std::make_pair(false, "Invalid time signature");
  }

  if (!d.HasMember(kMinimumNoteDurationTag) || !d[kMinimumNoteDurationTag].IsUint()) {
    return std::make_pair(false, "Missing/invalid minimum note duration");
  }

  minNoteValue = d[kMinimumNoteDurationTag].GetUint();

  if (minNoteValue == 0 || (minNoteValue & (minNoteValue - 1)) != 0) {
    return std::make_pair(false, "Min note value must be non-zero power of two");
  }

  switch (version) {
    case 1: {
      // Read tracks (can have none)
      numMeasures = 0;
      if (d.HasMember(kTracksTag) && d[kTracksTag].IsArray()) {
        const auto& tracksArray = d[kTracksTag];

        const auto& tracks = instrument->GetTracks();

        uint32 lastBeat = 0;
        uint32 lastLine = std::min(tracksArray.Size(), tracks.size());

        std::vector<Track*> tracksByIndex(tracks.size());
        for (const auto& track : tracks) {
          tracksByIndex[track.second->GetLoadIndex()] = track.second;
        }

        for (rapidjson::SizeType trackArrayIndex = 0; trackArrayIndex < lastLine; ++trackArrayIndex) {
          const auto& trackEntry = tracksArray[trackArrayIndex];

          // Read notes (can have none)
          if (!trackEntry.HasMember(kNotesTag) || !trackEntry[kNotesTag].IsArray()) {
            continue;
          }
          
          std::list<Note> notes;

          const auto& notesArray = trackEntry[kNotesTag];
          for (rapidjson::SizeType noteArrayIndex = 0; noteArrayIndex < notesArray.Size(); ++noteArrayIndex) {
            const auto& noteEntry = notesArray[noteArrayIndex];

            if (!noteEntry.HasMember(kBeatTag) || !noteEntry[kBeatTag].IsUint()) {
              MCLOG(Warn, "Invalid beat for note in notes array; skipping");
              continue;
            }

            auto beatIndex = noteEntry[kBeatTag].GetUint();

            uint32 gameLineIndex = kInvalidUint32;
            if (!noteEntry.HasMember(kFretTag) || !noteEntry[kFretTag].IsInt()) {
              MCLOG(Warn, "Invalid fret for note in notes array; defaulting");
            }
            else {
              gameLineIndex = static_cast<uint32>(noteEntry[kFretTag].GetInt());
            }

            notes.push_back(Note(beatIndex, gameLineIndex));

            if (lastBeat < beatIndex) {
              lastBeat = beatIndex;
            }
          }

          lines.insert({ tracksByIndex[trackArrayIndex]->GetUniqueId(), notes });

          // Match measure count up with time signature
          numMeasures = (lastBeat + (minNoteValue *
            beatsPerMeasure) - 1) / (minNoteValue * beatsPerMeasure);
        }
      }
      break;
    }
    case 2: {
      // Instrument object
      if (!d.HasMember(kInstrumentTag) || !d[kInstrumentTag].IsObject()) {
        return std::make_pair(false, "No instrument object in song file");
      }

      const auto& i = d[kInstrumentTag];

      if (!i.HasMember(Globals::kNameTag) || !i[Globals::kNameTag].IsString()) {
        return std::make_pair(false, "Missing/invalid instrument name");
      }

      std::string instrumentName = i[Globals::kNameTag].GetString();

      if (!i.HasMember(Globals::kPathTag) || !i[Globals::kPathTag].IsString()) {
        return std::make_pair(false, "Missing/invalid instrument path");
      }

      std::string instrumentPath = i[Globals::kPathTag].GetString();

      // Attempt to automatically load it
      instrument = Instrument::LoadInstrument(instrumentPath);
      if (!instrument) {
        if (!instrumentLoader) {
          return std::make_pair(false, "Unable to load instrument by path and no instrument loader callback provided");
        }
        instrument = instrumentLoader(instrumentName);
        if (!instrument) {
          return std::make_pair(false, "Unable to load required instrument to load song");
        }

      }

      // Read tracks (can have none)
      numMeasures = 0;
      if (i.HasMember(kTracksTag) && i[kTracksTag].IsArray()) {
        const auto& tracksArray = i[kTracksTag];

        uint32 lastBeat = 0;
        for (rapidjson::SizeType trackArrayIndex = 0; trackArrayIndex < tracksArray.Size(); ++trackArrayIndex) {
          const auto& trackEntry = tracksArray[trackArrayIndex];

          if (!trackEntry.HasMember(kTrackIdTag) || !trackEntry[kTrackIdTag].IsUint()) {
            MCLOG(Warn, "Missing/invalid name tag for track");
            continue;
          }

          auto trackId = trackEntry[kTrackIdTag].GetUint();

          if (!instrument->GetTrackById(trackId)) {
            MCLOG(Warn, "Song line %d refers to nonexistent track in instrument %s; skipping", trackArrayIndex, instrumentName.c_str());
            continue;
          }

          // Read notes (can have none)
          if (!trackEntry.HasMember(kNotesTag) || !trackEntry[kNotesTag].IsArray()) {
            continue;
          }

          std::list<Note> notes;

          const auto& notesArray = trackEntry[kNotesTag];
          for (rapidjson::SizeType noteArrayIndex = 0; noteArrayIndex < notesArray.Size(); ++noteArrayIndex) {
            const auto& noteEntry = notesArray[noteArrayIndex];

            if (!noteEntry.HasMember(kBeatTag) || !noteEntry[kBeatTag].IsUint()) {
              MCLOG(Warn, "Invalid beat for note in notes array; skipping");
              continue;
            }

            auto beatIndex = noteEntry[kBeatTag].GetUint();

            uint32 gameLineIndex = kInvalidUint32;
            if (noteEntry.HasMember(kGameLineTag)) {
              if (!noteEntry[kGameLineTag].IsUint()) {
                MCLOG(Warn, "Invalid game line for note in notes array; defaulting");
              }
              else {
                gameLineIndex = noteEntry[kGameLineTag].GetUint();
              }
            }

            notes.push_back(Note(beatIndex, gameLineIndex));

            if (lastBeat < beatIndex) {
              lastBeat = beatIndex;
            }
          }

          lines.insert({ trackId, notes });

          // Match measure count up with time signature
          numMeasures = (lastBeat + (minNoteValue *
            beatsPerMeasure) - 1) / (minNoteValue * beatsPerMeasure);
        }
      }

      break;
    }
    default:
      break;
  }

  return std::make_pair(true, "");
}

std::pair<bool, std::string> Song::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.StartObject();
  // Version tag:string
  w.Key(Globals::kVersionTag);
  w.Uint(kSongFileVersion);

  // Name
  w.Key(Globals::kNameTag);
  w.String(name.c_str());

  // Tempo
  w.Key(kTempoTag);
  w.Uint(GetTempo());

  w.Key(kTimeSignatureTag);
  // We currently assume a quarter-note base note value
  // https://trello.com/c/WY2eBMDh
  w.String((std::to_string(beatsPerMeasure) + "/" + std::to_string(kDefaultNoteValue)).c_str());

  w.Key(kMinimumNoteDurationTag);
  w.Uint(minNoteValue);

  // Instrument
  w.Key(kInstrumentTag);
  w.StartObject();

  // Instrument name
  w.Key(Globals::kNameTag);
  w.String(instrument->GetName().c_str());

  // Instrument path
  std::string serializeFileName(instrument->GetFileName());
  if (serializer.rootPath.generic_string().length()) {
    std::string newFileName = std::filesystem::relative(serializeFileName, serializer.rootPath).generic_string();
    if (newFileName.length() > 0) {
      serializeFileName = newFileName;

      // Everything should work with the incorrect (on Windows) forward-slash paths
      // returned from std::filesystem functions, but for consistency we'll convert
      // the result to Windows-style backslashes
      std::replace(serializeFileName.begin(), serializeFileName.end(), '/', '\\');
    }
    else {
      MCLOG(Warn, "Unable to localize instrument filename; song will refer to instrument by absolute path");
    }
  }
  w.Key(Globals::kPathTag);
  w.String(serializeFileName.c_str());
 
  if (GetNoteCount() != 0) {
    w.Key(kTracksTag);
    w.StartArray();

    for (const auto& line : lines) {
      w.StartObject();
      
      w.Key(kTrackIdTag);
      w.Uint(line.first);

      w.Key(kNotesTag);
      w.StartArray();

      for (const auto& note : line.second) {
        w.StartObject();

        w.Key(kBeatTag);
        w.Uint(note.GetBeatIndex());
        auto gameIndex = note.GetGameIndex();
        if (gameIndex != kInvalidUint32) {
          w.Key(kGameLineTag);
          w.Uint(gameIndex);
        }
        w.EndObject();
      }

      w.EndArray();
      w.EndObject();
    }

    w.EndArray();
  }

  w.EndObject();
  w.EndObject();

  return std::make_pair(true, "");
}

void Song::AddMeasures(uint32 numMeasures) {
  this->numMeasures += numMeasures;
}

Song::Note* Song::AddNote(uint32 trackId, uint32 beatIndex) {
  auto lineEntry = lines.find(trackId);
  assert(lineEntry != lines.end());
  if (lineEntry != lines.end()) {
    auto lineIter = lineEntry->second.begin();
    while (lineIter != lineEntry->second.end()) {
      if (lineIter->GetBeatIndex() > beatIndex) {
        return &(*lineEntry->second.insert(lineIter, Song::Note(beatIndex, -1)));
        break;
      }
      ++lineIter;
    }

    if (lineIter == lineEntry->second.end()) {
      return &(*lineEntry->second.insert(lineIter, Song::Note(beatIndex, -1)));
    }
  }
  return nullptr;
}

void Song::RemoveNote(uint32 trackId, uint32 beatIndex) {
  auto lineEntry = lines.find(trackId);
  assert(lineEntry != lines.end());
  if (lineEntry != lines.end()) {
    for (auto lineIter = lineEntry->second.begin(); lineIter != lineEntry->second.end(); ++lineIter) {
      if (lineIter->GetBeatIndex() == beatIndex) {
        lineEntry->second.erase(lineIter);
        break;
      }
    }
  }
}

std::string Song::GetInstrumentName() const {
  if (instrument != nullptr) {
    return instrument->GetName();
  }
  return {};
}

void Song::SetInstrument(Instrument* newInstrument) {
  delete instrument;

  lines.clear();
  numMeasures = kDefaultNumMeasures;
  
  instrument = newInstrument;
  
  if (instrument != nullptr) {
    const auto& tracks = instrument->GetTracks();
    for (const auto& track : tracks) {
      lines.insert({ track.second->GetUniqueId(), std::list<Note>() });
    }
  }
}

void Song::UpdateLines() {
  // Grab any new tracks
  const auto& tracks = instrument->GetTracks();
  for (const auto& track : tracks) {
    auto lineEntry = lines.find(track.first);
    if (lineEntry != lines.end()) {
      continue;
    }

    lines.insert({ track.first, std::list<Note>() });
  }
  // Discard any removed ones
  auto lineIter = lines.begin();
  while (lineIter != lines.end()) {
    auto trackEntry = tracks.find(lineIter->first);
    if (trackEntry != tracks.end()) {
      ++lineIter;
    }
    else {
      lineIter = lines.erase(lineIter);
    }
  }
}

bool Song::Save(std::string fileName) {
  ensure_fileext(fileName, Globals::kJsonTag);

  MCLOG(Info, "Saving song to file \'%s\'", fileName.c_str());

  rapidjson::StringBuffer sb;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);

  auto result = SerializeWrite({ w, std::filesystem::path(fileName).parent_path() });
  if (!result.first) {
    MCLOG(Error, "Unable to save song: %s", result.second.c_str());
    return false;
  }

  std::ofstream ofs(fileName);
  if (ofs.bad()) {
    MCLOG(Error, "Unable to save song to file %s ", fileName.c_str());
    return false;
  }

  std::string outputString(sb.GetString());
  ofs.write(outputString.c_str(), outputString.length());
  ofs.close();

  return true;
}

/* static */
Song* Song::LoadSongJson(std::string fileName, std::function<Instrument*(std::string)> instrumentLoader) {
  MCLOG(Info, "Loading song from file \'%s\'", fileName.c_str());

  std::ifstream ifs(fileName);

  if (ifs.bad()) {
    MCLOG(Error, "Unable to load song from file %s", fileName.c_str());
    return nullptr;
  }

  std::string fileData((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  if (!fileData.length()) {
    MCLOG(Error, "Unable to load song from file %s", fileName.c_str());
    return nullptr;
  }

  rapidjson::Document document;
  document.Parse(fileData.c_str());

  if (!document.IsObject()) {
    MCLOG(Error, "Failure parsing JSON in file %s", fileName.c_str());
    return nullptr;
  }

  try {
    auto newSong = new Song({ document, fileName }, instrumentLoader);

    if (newSong->GetMinNoteValue() > Globals::kDefaultMinNote) {
      delete newSong;
      MCLOG(Error, "Song subdivisions greater than sequencer max");
      return nullptr;
    }

    return newSong;
  }
  catch (std::runtime_error& rte) {
    MCLOG(Error, "Failed to load song: %s", rte.what());
  }

  return nullptr;
}

// TODO: Fix MIDI loading
// https://trello.com/c/vQCRzrcm
/* static */
Song* Song::LoadSongMidi(std::string fileName, std::function<Instrument*(std::string)> instrumentLoader) {
#if 0
  if (!instrument) {
    MCLOG(Error, "Cannot load MIDI file without a loaded instrument");
    return;
  }

  if (!instrument->tracks.size()) {
    MCLOG(Error, "Cannot load MIDI file if instrument has no tracks");
    return;
  }

  MCLOG(Info, "Importing MIDI from file \'%s\'", fileName.c_str());

  MidiSource midiSource;
  if (!midiSource.openFile(fileName)) {
    MCLOG(Warn, "Failed to load MIDI file \'%s\'", fileName.c_str());
    return;
  }

  if (!midiSource.getTrackCount()) {
    MCLOG(Warn, "Loading MIDI file \'%s\' resulted in no tracks", fileName.c_str());
    return;
  }

  if (!midiConversionParamsCallback) {
    MCLOG(Error, "MIDI file \'%s\' loaded but no callback provided", fileName.c_str());
    return;
  }

  MidiConversionParams midiConversionParams;
  if (midiConversionParamsCallback(midiSource, midiConversionParams)) {
    MidiTrack midiTrack;

    static constexpr uint32 kMinMidiValue = 21;

    // Yikes
    uint32 numMeasures = 1;
    if (midiSource.CombineTracks(midiTrack, midiConversionParams.trackIndices)) {
      // Ok. We now have all the tracks we want in one track; it is only note-ons;
      // they are globally and locally time stamped
      // Now we need to iterate these and add them as notes!
      for (const auto& midiEvent : midiTrack.events) {
        auto a = static_cast<int32>(midiEvent.dataptr[1]);
        auto b = static_cast<int32>(kMinMidiValue);
        auto trackIndex = (std::max(a, b) - b) % instrument->tracks.size();
        auto beatsIndex = static_cast<uint32>(static_cast<double>(midiEvent.timeStamp) /
          static_cast<double>(midiSource.getTimeDivision()) * Globals::kDefaultMinNote);

        if (!(beatsIndex % Globals::kDefaultMinNote)) {
          ++numMeasures;
        }

        //instrument->SetTrackNote(trackIndex, beatsIndex, 1.0f);
      }

      MCLOG(Info, "Successfully loaded MIDI file \'%s\'", fileName.c_str());
    }
    else {
      MCLOG(Error, "Failure while combining MIDI tracks");
    }

  }
  else {
    MCLOG(Error, "Failure while converting MIDI");
  }
#endif
  return nullptr;
}

/* static */
Song* Song::LoadSong(std::string fileName, std::function<Instrument*(std::string)> instrumentLoader) {
  if (check_fileext(fileName, Globals::kJsonTag)) {
    return LoadSongJson(fileName, instrumentLoader);
  }
  for (size_t m = 0; m < _countof(Globals::kMidiTags); ++m) {
    if (check_fileext(fileName, Globals::kMidiTags[m])) {
      return LoadSongMidi(fileName, instrumentLoader);
    }
  }
  return nullptr;
}


