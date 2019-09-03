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
  , beatsPerMeasure(beatsPerMeasure)
  , minNoteValue(minNoteValue) {
  barLines.resize(numLines);
  for (auto& barLine : barLines) {
    barLine.resize(numMeasures * beatsPerMeasure * minNoteValue);
  }
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
    beatsPerMeasure = std::stoi(timeSignature.substr(0, div));

    // We currently assume a quarter-note base note value
    // https://trello.com/c/WY2eBMDh

    // TODO: Validation of time signature
  }
  else {
    return std::make_pair(false, "Invalid time signature");
  }

  if (!d.HasMember(kMinimumNoteDurationTag) || !d[kMinimumNoteDurationTag].IsUint()) {
    return std::make_pair(false, "Missing/invalid minimum note duration");
  }

  minNoteValue = d[kMinimumNoteDurationTag].GetUint();

  // Read tracks (can have none)
  if (d.HasMember(kTracksTag) && d[kTracksTag].IsArray()) {
    const auto& tracksArray = d[kTracksTag];

    barLines.resize(tracksArray.Size());

    uint32 maxNote = 0;
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

        if (barLines[trackArrayIndex].size() <= beatIndex) {
          barLines[trackArrayIndex].resize(beatIndex + 1);
        }

        barLines[trackArrayIndex][beatIndex] = Note(true, fretIndex);
      }

      if (maxNote < barLines[trackArrayIndex].size()) {
        maxNote = barLines[trackArrayIndex].size();
      }
    }

    // Make sure we have full measures and pad out all the lines
    uint32 minNotesPerMeasure = minNoteValue * beatsPerMeasure;
    if ((maxNote % minNotesPerMeasure) != 0) {
      auto noteCount = ((maxNote / minNotesPerMeasure) + 1) * minNotesPerMeasure;
      for (auto& barLine : barLines) {
        barLine.resize(noteCount);
      }
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

      for (size_t n = 0; n < line.size(); ++n) {
        auto& note = line[n];
        if (note.GetEnabled()) {
          w.StartObject();

          w.Key(kBeatTag);
          w.Uint(n);
          w.Key(kFretTag);
          w.Int(note.GetGameIndex());

          w.EndObject();
        }
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
  for (auto& line : barLines) {
    line.resize(line.size() + numMeasures * minNoteValue * beatsPerMeasure);
  }
}

void Song::AddLine() {
  barLines.push_back(std::vector<Note>(GetNoteCount()));
}

void Song::RemoveLine(uint32 lineIndex) {
  barLines.erase(barLines.begin() + lineIndex);
}

void Song::ToggleNoteEnabled(uint32 gameIndex, uint32 noteIndex) {
  auto& note = barLines[gameIndex][noteIndex];
  note.SetEnabled(!note.GetEnabled());
}

void Song::SetNoteGameIndex(uint32 lineIndex, uint32 noteIndex, int32 gameIndex) {
  auto& note = barLines[lineIndex][noteIndex];
  note.SetGameIndex(gameIndex);
}


