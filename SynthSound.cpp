#include "SynthSound.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include "SerializeImpl.h"
#include "Logging.h"
#include "Mixer.h"

static constexpr const char* kFrequencyTag("frequency");
static constexpr const char* kDurationTag("duration");

SynthSound::SynthSound(const ReadSerializer& serializer) 
: Sound("SynthSound") {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Failed to serialize SynthSound");
  }
}

bool SynthSound::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  // Frequency tag:uint
  w.Key(kFrequencyTag);
  w.Uint(frequency);

  // Duration tag:uint
  w.Key(kDurationTag);
  w.Uint(duration);

  return true;
}

bool SynthSound::SerializeRead(const ReadSerializer& serializer) {
  auto& r = serializer.d;

  // Frequency
  if (!r.HasMember(kFrequencyTag) || !r[kFrequencyTag].IsUint()) {
    MCLOG(Warn, "Missing/invalid frequency tag");
    return false;
  }
  frequency = r[kFrequencyTag].GetUint();

  // Duration
  if (!r.HasMember(kDurationTag) || !r[kDurationTag].IsUint()) {
    MCLOG(Warn, "Missing/invalid frequency tag");
    return false;
  }
  duration = r[kDurationTag].GetUint();

  return true;
}

// Sine
REGISTER_SYNTH_SOUND(SineSynthSound, "Modulated sine wave");
SineSynthSound::SineSynthSound(uint32 frequency, uint32 duration)
  : SynthSound("SineSynthSound", frequency, duration) {
}

Voice* SineSynthSound::CreateVoice() {
  SineSynthVoice* voice = new SineSynthVoice;
  voice->radstep = static_cast<float>((2.0 * M_PI *
    frequency) / static_cast<double>(Mixer::kDefaultFrequency));
  return voice;
}

uint8 SineSynthSound::GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voiceGeneric) {
  if (frame < duration) {
    SineSynthVoice* voice = static_cast<SineSynthVoice*>(voiceGeneric);

    voice->radians += voice->radstep; // * speed
    while (voice->radians > (2.0 * M_PI)) {
      voice->radians -= static_cast<float>(2.0 * M_PI);
    }

    float s = sinf(voice->radians);
    for (uint8 channel = 0; channel < channels; ++channel) {
      samples[channel] = s;
    }
    return channels;
  }
  return 0;
}
