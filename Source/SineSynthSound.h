#pragma once

#include "SynthSound.h"

class SineSynthSound : public SynthSound {
public:
  SineSynthSound();
  SineSynthSound(const SineSynthSound& that);
  SineSynthSound(uint32 frequency, float duration);
  SineSynthSound(const ReadSerializer& serializer);

  Sound* Clone() override;

  void RenderDialog() override;
};
