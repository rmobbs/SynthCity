#include "InstrumentInstance.h"
#include "Instrument.h"
#include "OddsAndEnds.h"

#include <assert.h>

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
    lines.insert({ track.first, {} });

    UniqueIdBuilder<64> trackHamburgerMenuIdBuilder("thm:");
    trackHamburgerMenuIdBuilder.PushHex(reinterpret_cast<uint32>(track.second));
    trackHamburgerMenuIdBuilder.PushHex(track.first);
    UniqueIdBuilder<64> trackPropertiesPopIdBuilder("tpr:");
    trackPropertiesPopIdBuilder.PushHex(reinterpret_cast<uint32>(track.second));
    trackPropertiesPopIdBuilder.PushHex(track.first);

    songTracks.insert({ track.first,
      {
        trackHamburgerMenuIdBuilder(),
        trackPropertiesPopIdBuilder(),
        track.first,
      { },
      false,
      }
      });
  }
}

InstrumentInstance::~InstrumentInstance() {

}

Note* InstrumentInstance::AddNote(uint32 trackId, uint32 beatIndex) {
  auto lineEntry = lines.find(trackId);
  assert(lineEntry != lines.end());
  if (lineEntry != lines.end()) {
    auto lineIter = lineEntry->second.begin();
    while (lineIter != lineEntry->second.end()) {
      if (lineIter->GetBeatIndex() > beatIndex) {
        return &(*lineEntry->second.insert(lineIter, Note(beatIndex, -1)));
        break;
      }
      ++lineIter;
    }

    if (lineIter == lineEntry->second.end()) {
      return &(*lineEntry->second.insert(lineIter, Note(beatIndex, -1)));
    }
  }
  return nullptr;
}

void InstrumentInstance::RemoveNote(uint32 trackId, uint32 beatIndex) {
  {
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
  {
    auto lineEntry = songTracks.find(trackId);
    if (lineEntry != songTracks.end()) {
      lineEntry->second.notes[beatIndex].note = nullptr;
    }
  }
}

void InstrumentInstance::SetNoteGameIndex(uint32 trackId, uint32 beatIndex, int32 gameIndex) {
  auto track = songTracks.find(trackId);
  assert(track != songTracks.end());
  track->second.notes[beatIndex].note->SetGameIndex(gameIndex);
}

void InstrumentInstance::SetTrackMute(uint32 trackId, bool mute) {
  auto track = songTracks.find(trackId);
  assert(track != songTracks.end());
  track->second.mute = mute;
}
