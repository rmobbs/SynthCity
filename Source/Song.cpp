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
static constexpr uint32 kSongFileVersion = 1;
static constexpr uint32 kDefaultNoteValue = 4;

Song::Song(std::string name, uint32 tempo, uint32 numLines, uint32 numMeasures, uint32 beatsPerMeasure, uint32 minNoteValue)
  : name(name)
  , tempo(tempo)
  , numMeasures(numMeasures)
  , beatsPerMeasure(beatsPerMeasure)
  , minNoteValue(minNoteValue) {
  barLines.resize(numLines);
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
      // Single instrument name
      if (!d.HasMember(kInstrumentTag) || !d[kInstrumentTag].IsString()) {
        return std::make_pair(false, "Missing instrument tag");
      }

      std::string instrumentName = d[kInstrumentTag].GetString();

      // They'll have to tell us where to load it from
      instrument = instrumentLoader(instrumentName);
      if (!instrument) {
        return std::make_pair(false, "Unable to load required instrument to load song");
      }
      MCLOG(Warn, "Song is version 1 and this will be deprecated");
      break;
    }
    case 2: {
      // Single instrument path
      if (!d.HasMember(kInstrumentTag) || !d[kInstrumentTag].IsString()) {
        return std::make_pair(false, "Missing instrument tag");
      }

      std::string instrumentPath = d[kInstrumentTag].GetString();

      // They'll have to tell us where to load it from
      instrument = Instrument::LoadInstrument(instrumentPath);
      if (!instrument) {
        return std::make_pair(false, "Unable to load required instrument to load song");
      }
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

  // Read tracks (can have none)
  numMeasures = 0;
  if (d.HasMember(kTracksTag) && d[kTracksTag].IsArray()) {
    const auto& tracksArray = d[kTracksTag];

    barLines.resize(tracksArray.Size());

    uint32 lastBeat = 0;
    for (rapidjson::SizeType trackArrayIndex = 0; trackArrayIndex < tracksArray.Size(); ++trackArrayIndex) {
      const auto& trackEntry = tracksArray[trackArrayIndex];

      // Read notes (can have none)
      if (!trackEntry.HasMember(kNotesTag) || !trackEntry[kNotesTag].IsArray()) {
        continue;
      }

      const auto& notesArray = trackEntry[kNotesTag];
      for (rapidjson::SizeType noteArrayIndex = 0; noteArrayIndex < notesArray.Size(); ++noteArrayIndex) {
        const auto& noteEntry = notesArray[noteArrayIndex];

        if (!noteEntry.HasMember(kBeatTag) || !noteEntry[kBeatTag].IsUint()) {
          MCLOG(Warn, "Invalid beat for note in notes array; skipping");
          continue;
        }

        auto beatIndex = noteEntry[kBeatTag].GetUint();

        int32 fretIndex = -1;
        if (!noteEntry.HasMember(kFretTag) || !noteEntry[kFretTag].IsInt()) {
          MCLOG(Warn, "Invalid fret for note in notes array; defaulting");
        }
        else {
          fretIndex = noteEntry[kFretTag].GetInt();
        }

        barLines[trackArrayIndex].push_back(Note(beatIndex, fretIndex));

        if (lastBeat < beatIndex) {
          lastBeat = beatIndex;
        }
      }

      // Match measure count up with time signature
      numMeasures = (lastBeat + (minNoteValue *
        beatsPerMeasure) - 1) / (minNoteValue * beatsPerMeasure);
    }
  }

  return std::make_pair(true, "");
}

std::pair<bool, std::string> Song::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.StartObject();
  // Version tag:string
  w.Key(Globals::kVersionTag);
  w.Uint(kSongFileVersion);

  w.Key(kInstrumentTag);
  w.String(instrument->GetName().c_str());

  w.Key(kTempoTag);
  w.Uint(GetTempo());

  w.Key(kTimeSignatureTag);
  // We currently assume a quarter-note base note value
  // https://trello.com/c/WY2eBMDh
  w.String((std::to_string(beatsPerMeasure) + "/" + std::to_string(kDefaultNoteValue)).c_str());

  w.Key(kMinimumNoteDurationTag);
  w.Uint(minNoteValue);

  if (GetNoteCount() != 0) {
    w.Key(kTracksTag);
    w.StartArray();

    for (uint32 lineIndex = 0; lineIndex < barLines.size(); ++lineIndex) {
      auto line = barLines[lineIndex];

      w.StartObject();

      w.Key(kNotesTag);
      w.StartArray();

      for (const auto& note : line) {
        w.StartObject();

        w.Key(kBeatTag);
        w.Uint(note.GetBeatIndex());
        w.Key(kFretTag);
        w.Int(note.GetGameIndex());

        w.EndObject();
      }

      w.EndArray();
      w.EndObject();
    }

    w.EndArray();
    w.EndObject();
  }

  return std::make_pair(true, "");
}

void Song::AddMeasures(uint32 numMeasures) {
  this->numMeasures += numMeasures;
}

void Song::AddLine() {
  barLines.resize(barLines.size() + 1);
}

void Song::RemoveLine(uint32 lineIndex) {
  barLines.erase(barLines.begin() + lineIndex);
}

Song::Note* Song::AddNote(uint32 lineIndex, uint32 beatIndex) {
  auto& line = barLines[lineIndex];

  auto lineIter = line.begin();
  while (lineIter != line.end()) {
    if (lineIter->GetBeatIndex() > beatIndex) {
      return &(*line.insert(lineIter, Song::Note(beatIndex, -1)));
      break;
    }
    ++lineIter;
  }

  if (lineIter == line.end()) {
    return &(*line.insert(lineIter, Song::Note(beatIndex, -1)));
  }

  return nullptr;
}

void Song::RemoveNote(uint32 lineIndex, uint32 beatIndex) {
  auto& line = barLines[lineIndex];

  for (auto lineIter = line.begin(); lineIter != line.end(); ++lineIter) {
    if (lineIter->GetBeatIndex() == beatIndex) {
      line.erase(lineIter);
      break;
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
  barLines.clear();
  numMeasures = 0;
  instrument = newInstrument;
  if (instrument != nullptr) {
    barLines.resize(instrument->GetTrackCount());
    numMeasures = kDefaultNumMeasures;
    for (auto& barLine : barLines) {
      barLine.resize(GetNoteCount());
    }
  }
}

bool Song::Save(std::string fileName) {
  ensure_fileext(fileName, Globals::kJsonTag);

  MCLOG(Info, "Saving song to file \'%s\'", fileName.c_str());

  rapidjson::StringBuffer sb;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);

  auto result = SerializeWrite({ w });
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
    auto newSong = new Song({ document }, instrumentLoader);

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


