#include "SineSynthSound.h"
#include "SoundFactory.h"
#include "Sequencer.h"
#include "imgui.h"
#include <algorithm>

// F's sake, just make a polymorphic constant, C++ standards committee!!
#define _USE_MATH_DEFINES
#include <math.h>

static constexpr uint32 kSineSynthSoundInstancePoolSize = 128;

// Sine
class SineSynthSoundInstance : public SoundInstance {
protected:
  float radians = 0;
  float radstep = 0;
public:
  using SoundInstance::SoundInstance;

  SineSynthSoundInstance(Sound* sound)
    : SoundInstance(sound) {
    radstep = static_cast<float>((2.0 * M_PI * static_cast<double>
      (static_cast<SynthSound*>(sound)->GetFrequency())) / static_cast<double>(Sequencer::Get().GetFrequency()));
  }

  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame) override {
    if (frame < static_cast<uint32>(sound->GetDuration() * 44100.0f)) {
      radians += radstep;
      while (radians > (2.0 * M_PI)) {
        radians -= static_cast<float>(2.0 * M_PI);
      }

      float s = sinf(radians);
      for (uint8 channel = 0; channel < channels; ++channel) {
        samples[channel] = s;
      }
      return channels;
    }
    return 0;
  }
};

// The pool size should be relatively big b/c multiple instances can play
// simultaneously
REGISTER_SOUND_INSTANCE(SineSynthSoundInstance, SineSynthSound, kSineSynthSoundInstancePoolSize);

REGISTER_SOUND(SineSynthSound, "Modulated sine wave");
SineSynthSound::SineSynthSound()
  : SynthSound("SineSynthSound") {

}

SineSynthSound::SineSynthSound(const SineSynthSound& that)
  : SynthSound(that) {

}

SineSynthSound::SineSynthSound(uint32 frequency, float duration)
  : SynthSound("SineSynthSound", frequency, duration) {
}

SineSynthSound::SineSynthSound(const ReadSerializer& serializer)
  : SynthSound("SineSynthSound", serializer) {
}

Sound* SineSynthSound::Clone() {
  return new SineSynthSound(*this);
}

void SineSynthSound::RenderDialog() {
  int frequencyInt = frequency;
  if (ImGui::InputInt("Frequency", &frequencyInt)) {
    frequency = std::min(std::max(static_cast<uint32>(frequencyInt), kMinFrequency), kMaxFrequency);
  }

  float durationTmp = duration;
  if (ImGui::InputFloat("Duration", &durationTmp)) {
    if (durationTmp < std::numeric_limits<float>::epsilon()) {
      durationTmp = std::numeric_limits<float>::epsilon();
    }
    duration = durationTmp;
  }
}
