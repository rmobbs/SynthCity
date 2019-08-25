#include "Song.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Globals.h"

static constexpr const char *kInstrumentTag = "Instrument";
static constexpr const char *kTempoTag = "Tempo";
static constexpr const char *kMeasuresTag = "Measures";
static constexpr const char *kTimeSignatureTag = "TimeSignature";
static constexpr const char* kTracksTag = "Tracks";
static constexpr const char *kNotesTag = "Notes";
static constexpr const char *kBeatTag = "Beat";
static constexpr const char* kFretTag = "Fret";
static constexpr const char* kMinimumNoteDurationTag = "MinimumNoteDuration";

Song::Song(uint32 numLines, uint32 tempo, uint32 numMeasures, uint32 beatsPerMeasure, uint32 beatSubdivision)
  : tempo(tempo)
  , beatsPerMeasure(beatsPerMeasure)
  , beatSubdivision(beatSubdivision) {
  barLines.resize(numLines);
  for (auto& barLine : barLines) {
    barLine.resize(numMeasures * beatsPerMeasure * beatSubdivision);
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
  if (!d.HasMember(Globals::kVersionTag) || !d[Globals::kVersionTag].IsString()) {
    return std::make_pair(false, "Missing/invalid version tag");
  }

  std::string version = d[Globals::kVersionTag].GetString();

  if (version != std::string(Globals::kVersionString)) {
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
    noteValue = std::stoi(timeSignature.substr(div + 1));

    // TODO: Validation of time signature
  }
  else {
    return std::make_pair(false, "Invalid time signature");
  }

  if (!d.HasMember(kMinimumNoteDurationTag) || !d[kMinimumNoteDurationTag].IsUint()) {
    return std::make_pair(false, "Missing/invalid minimum note duration");
  }

  beatSubdivision = d[kMinimumNoteDurationTag].GetUint();

  // Read tracks (can have none)
  if (d.HasMember(kTracksTag) && d[kTracksTag].IsArray()) {
    const auto& tracksArray = d[kTracksTag];

    barLines.resize(tracksArray.Size());

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
    }

    // Make sure we have full measures
    if (barLines.size()) {
      uint32 beatCount = barLines[0].size();

      uint32 minBeatsPerMeasure = beatSubdivision * beatsPerMeasure;
      if ((beatCount % minBeatsPerMeasure) != 0) {
        beatCount = ((beatCount / minBeatsPerMeasure) + 1) * minBeatsPerMeasure;
        for (auto& barLine : barLines) {
          barLine.resize(beatCount);
        }
      }
    }
  }

  return std::make_pair(true, "");
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


