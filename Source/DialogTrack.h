#pragma once

#include "Dialog.h"
#include "BaseTypes.h"
#include "SerializeFwd.h"
#include <vector>

class Track;
class DialogTrack : public Dialog {
protected:
  Track* track;

public:
  DialogTrack(Track* track);
  ~DialogTrack();

  void Open() override;
  bool Render() override;

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

