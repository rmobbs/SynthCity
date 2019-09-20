#pragma once

#include "Dialog.h"
#include "BaseTypes.h"
#include "SerializeFwd.h"
#include <vector>
#include <string>

class Instrument;
class Track;
class DialogTrack : public Dialog {
protected:
  std::string title;
  uint32 replaceTrackId = kInvalidUint32;
  int32 playingVoiceId = -1;
  Instrument* instrument = nullptr;
  Track* track = nullptr;
  uint32 stopButtonTexture = 0xFFFFFFFF;
public:
  DialogTrack(std::string title, Instrument* instrument, uint32 replaceTrackId, Track* track, uint32 stopButtonTexture);
  ~DialogTrack();

  void Open() override;
  bool Render() override;
};

