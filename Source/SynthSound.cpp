#include "SynthSound.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Mixer.h"
#include "SoundFactory.h"
#include "Sequencer.h"
#include <algorithm>
#include "imgui.h"

static constexpr const char* kFrequencyTag("frequency");
static constexpr const char* kDurationTag("duration");

SynthSound::SynthSound(const SynthSound& that)
  : Sound(that) {

}

SynthSound::SynthSound(const std::string& className)
  : Sound(className) {

}

SynthSound::SynthSound(const std::string& className, uint32 frequency, float duration)
  : Sound(className)
  , frequency(frequency) {
  this->duration = duration;
}

SynthSound::SynthSound(const std::string& className, const ReadSerializer& serializer)
  : Sound(className) {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Unable to serialize synth sound");
  }
}

bool SynthSound::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  // Frequency tag:uint
  w.Key(kFrequencyTag);
  w.Uint(frequency);

  // Duration tag:uint
  w.Key(kDurationTag);
  w.Double(duration);

  return true;
}

bool SynthSound::SerializeRead(const ReadSerializer& serializer) {
  auto& r = serializer.d;

  // Frequency
  if (!r.HasMember(kFrequencyTag) || !r[kFrequencyTag].IsUint()) {
    MCLOG(Warn, "Missing/invalid frequency");
    return false;
  }
  frequency = r[kFrequencyTag].GetUint();

  // Duration
  if (!r.HasMember(kDurationTag) || !r[kDurationTag].IsDouble()) {
    MCLOG(Warn, "Missing/invalid duration");
    return false;
  }
  duration = static_cast<float>(r[kDurationTag].GetDouble());

  return true;
}
