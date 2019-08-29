#pragma once

#include "Dialog.h"
#include "BaseTypes.h"
#include "SerializeFwd.h"
#include <vector>
#include <string>

class Track;
class DialogTrack : public Dialog {
protected:
  std::string title;
  int32 trackIndex = -1;
  int32 playingVoiceId = -1;
  Track* track = nullptr;
  uint32 stopButtonTexture = 0xFFFFFFFF;
public:
  DialogTrack(std::string title, int32 trackIndex, Track* track, uint32 stopButtonTexture);
  ~DialogTrack();

  void Open() override;
  bool Render() override;
};

