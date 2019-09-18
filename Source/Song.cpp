#include "Song.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Globals.h"

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

Song::Song(uint32 numLines, uint32 tempo, uint32 numMeasures, uint32 beatsPerMeasure, uint32 minNoteValue)
  : tempo(tempo)
  , numMeasures(numMeasures)
  , beatsPerMeasure(beatsPerMeasure)
  , minNoteValue(minNoteValue) {
  barLines.resize(numLines);
}

Song::Song(const ReadSerializer& serializer) {
  auto result = SerializeRead(serializer);
  if (!result.first) {
    throw std::runtime_error("Failed to serialize (read): " + result.second);
  }
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

  if (version != kSongFileVersion) {
    // Allow conversion of previous formats
    // https://trello.com/c/O0SzcHfG
    return std::make_pair(false, "Invalid song file version");
  }

  // Get instrument (TODO: allow multiple instruments?)
  if (!d.HasMember(kInstrumentTag) || !d[kInstrumentTag].IsString()) {
    return std::make_pair(false, "Missing instrument tag");
  }

  instrumentName = d[kInstrumentTag].GetString();

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
  w.String(instrumentName.c_str());

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



