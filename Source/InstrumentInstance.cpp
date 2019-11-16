#include "InstrumentInstance.h"
#include "Instrument.h"
#include "OddsAndEnds.h"
#include "Sequencer.h"
#include "Song.h"
#include <assert.h>

TrackInstance::GuiNote::GuiNote() {
  auto nextUniqueNoteId = Sequencer::Get().GetSong()->NextUniqueNoteId();

  UniqueIdBuilder<64> noteUniqueIdBuilder("tnt:");
  noteUniqueIdBuilder.PushUnsigned(nextUniqueNoteId);
  uniqueGuiId = noteUniqueIdBuilder();
}

TrackInstance::TrackInstance(uint32 trackId) {
  this->trackId = trackId;

  auto nextUniqueTrackId = Sequencer::Get().GetSong()->NextUniqueTrackId();

  UniqueIdBuilder<64> uniqueIdBuilder("thm:");
  uniqueIdBuilder.PushUnsigned(nextUniqueTrackId);
  uniqueGuiIdHamburgerMenu = uniqueIdBuilder();

  uniqueIdBuilder = { "tpr:" };
  uniqueIdBuilder.PushUnsigned(nextUniqueTrackId);
  uniqueGuiIdPropertiesPop = uniqueIdBuilder();

  uniqueIdBuilder = { "tbt:" };
  uniqueIdBuilder.PushUnsigned(nextUniqueTrackId);
  uniqueGuiIdTrackButton = uniqueIdBuilder();
}

InstrumentInstance::InstrumentInstance(Instrument* instrument) :
  instrument(instrument) {
  auto nextUniqueInstrumentInstanceId = Sequencer::Get().GetSong()->NextUniqueInstrumentInstanceId();

  // Generate GUI IDs for instrument
  UniqueIdBuilder<64> uniqueIdBuilder("inm:");
  uniqueIdBuilder.PushUnsigned(nextUniqueInstrumentInstanceId);
  uniqueGuiIdName = uniqueIdBuilder();

  uniqueIdBuilder = { "ihm:" };
  uniqueIdBuilder.PushUnsigned(nextUniqueInstrumentInstanceId);
  uniqueGuiIdHamburgerMenu = uniqueIdBuilder();

  uniqueIdBuilder = { "ipr:" };
  uniqueIdBuilder.PushUnsigned(nextUniqueInstrumentInstanceId);
  uniqueGuiIdPropertiesPop = uniqueIdBuilder();

  // Create tracks
  for (const auto& track : instrument->GetTracks()) {
    trackInstances.insert({ track.first, { track.first } });
  }
}

InstrumentInstance::~InstrumentInstance() {

}

Note* InstrumentInstance::AddNote(uint32 trackId, uint32 beatIndex, int32 gameIndex) {
  Note* note = nullptr;

  auto trackInstance = trackInstances.find(trackId);
  assert(trackInstance != trackInstances.end());
  auto lineIter = trackInstance->second.noteList.begin();
  while (lineIter != trackInstance->second.noteList.end()) {
    if (lineIter->GetBeatIndex() > beatIndex) {
      note = &(*trackInstance->second.noteList.insert(lineIter, Note(beatIndex, gameIndex)));
      break;
    }
    ++lineIter;
  }

  if (lineIter == trackInstance->second.noteList.end()) {
    note = &(*trackInstance->second.noteList.insert(lineIter, Note(beatIndex, gameIndex)));
  }

  assert(note != nullptr);

  EnsureTrackNotes(trackInstance->second, trackId, beatIndex + 1);

  if (trackInstance->second.noteVector.size() <= beatIndex) {
    trackInstance->second.noteVector.resize(beatIndex + 1);
  }
  trackInstance->second.noteVector[beatIndex].note = note;

  return nullptr;
}

void InstrumentInstance::RemoveNote(uint32 trackId, uint32 beatIndex) {
  auto trackInstance = trackInstances.find(trackId);
  assert(trackInstance != trackInstances.end());
  for (auto lineIter = trackInstance->second.noteList.begin(); lineIter != trackInstance->second.noteList.end(); ++lineIter) {
    if (lineIter->GetBeatIndex() == beatIndex) {
      trackInstance->second.noteList.erase(lineIter);
      break;
    }
  }
  trackInstance->second.noteVector[beatIndex].note = nullptr;
}

void InstrumentInstance::EnsureTrackNotes(TrackInstance& trackInstance, uint32 trackId, uint32 noteCount) {
  if (trackInstance.noteVector.size() < noteCount) {
    trackInstance.noteVector.resize(noteCount);
  }
}

void InstrumentInstance::EnsureNotes(uint32 noteCount) {
  for (auto& trackInstance : trackInstances) {
    EnsureTrackNotes(trackInstance.second, trackInstance.first, noteCount);
  }
}

void InstrumentInstance::SetNoteGameIndex(uint32 trackId, uint32 beatIndex, int32 gameIndex) {
  auto trackInstance = trackInstances.find(trackId);
  assert(trackInstance != trackInstances.end());
  trackInstance->second.noteVector[beatIndex].note->SetGameIndex(gameIndex);
}

void InstrumentInstance::SetTrackMute(uint32 trackId, bool mute) {
  auto trackInstance = trackInstances.find(trackId);
  assert(trackInstance != trackInstances.end());
  trackInstance->second.mute = mute;
}
