#include "InstrumentInstance.h"
#include "Instrument.h"
#include "OddsAndEnds.h"

#include <assert.h>

TrackInstance::TrackInstance(uint32 trackId) {
  UniqueIdBuilder<64> trackHamburgerMenuIdBuilder("thm:");
  trackHamburgerMenuIdBuilder.PushHex(reinterpret_cast<uint32>(this));
  uniqueGuiIdHamburgerMenu = trackHamburgerMenuIdBuilder();

  UniqueIdBuilder<64> trackPropertiesPopIdBuilder("tpr:");
  trackPropertiesPopIdBuilder.PushHex(reinterpret_cast<uint32>(this));
  uniqueGuiIdPropertiesPop = trackPropertiesPopIdBuilder();
}

InstrumentInstance::InstrumentInstance(Instrument* instrument) :
  instrument(instrument) {
  // Generate GUI IDs for instrument
  UniqueIdBuilder<64> uniqueIdBuilder("name:");
  uniqueIdBuilder.PushHex(reinterpret_cast<uint32>(this));
  uniqueGuiIdName = uniqueIdBuilder();

  uniqueIdBuilder = { "thm:" };
  uniqueIdBuilder.PushHex(reinterpret_cast<uint32>(this));
  uniqueGuiIdHamburgerMenu = uniqueIdBuilder();

  uniqueIdBuilder = { "tpr:" };
  uniqueIdBuilder.PushHex(reinterpret_cast<uint32>(this));
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
  UniqueIdBuilder<64> uniqueIdBuilder("nt:");
  uniqueIdBuilder.PushHex(reinterpret_cast<uint32>(this));
  uniqueIdBuilder.PushUnsigned(trackId);

  auto noteIndex = trackInstance.noteVector.size();
  while (noteIndex < noteCount) {
    uniqueIdBuilder.PushUnsigned(noteIndex);
    trackInstance.noteVector.push_back({ nullptr, uniqueIdBuilder() });
    uniqueIdBuilder.Pop();
    ++noteIndex;
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
