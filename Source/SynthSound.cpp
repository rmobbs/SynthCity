#include "SynthSound.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Mixer.h"
#include "SoundFactory.h"
#include "Sequencer.h"
#include "resource.h"
#include <algorithm>
#include "imgui.h"

static constexpr const char* kFrequencyTag("frequency");
static constexpr const char* kDurationTag("duration");

SynthSound::SynthSound(std::string className, const ReadSerializer& serializer)
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
  w.Uint(durationNum << 16 | durationDen);

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
  uint32 durationMasked = r[kDurationTag].GetUint();

  durationNum = durationMasked >> 16;
  durationDen = durationMasked & 0xFFFF;

  return true;
}

REGISTER_DIALOG(DialogSynthSound);
bool DialogSynthSound::Render() {
  ImGui::Text("DialogSynthSound");
  return true;
}

bool DialogSynthSound::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  // Frequency
  uint32 frequency = 1000;
  w.Key("frequency");
  w.Uint(frequency);

  // The time value of the note unit in which our duration is expressed, i.e. half-note, quarter-note
  // We'll make sure it's between [2,<upper-limit>] and it's even
  INT den = 4;
  den = std::min(std::max(static_cast<uint32>(den), 2u), Sequencer::Get().GetMaxSubdivisions()) & ~1u;

  // The number of these notes that defines the duration
  INT num = 2;

  w.Key("duration");
  w.Uint(num << 16 | den);
  return true;
}

bool DialogSynthSound::SerializeRead(const ReadSerializer& serializer) {
  return true;
}

// Sine
REGISTER_SOUND(SineSynthSound, "Modulated sine wave", DialogSynthSound);
SineSynthSound::SineSynthSound(uint32 frequency, uint32 durationNum, uint32 durationDen)
  : SynthSound("SineSynthSound", frequency, durationNum, durationDen) {
}

SineSynthSound::SineSynthSound(const ReadSerializer& serializer)
  : SynthSound("SineSynthSound", serializer) {
}

SoundInstance* SineSynthSound::CreateInstance() {
  SineSynthSoundInstance* instance = new SineSynthSoundInstance;

  float durationInSeconds = static_cast<float>(Sequencer::Get().
    GetSecondsPerBeat()) * (static_cast<float>(durationNum) / static_cast<float>(durationDen));

  instance->duration = static_cast<uint32>(durationInSeconds * static_cast<float>(Mixer::kDefaultFrequency));

  instance->radstep = static_cast<float>((2.0 * M_PI *
    frequency) / static_cast<double>(Mixer::kDefaultFrequency));
  return instance;
}

uint8 SineSynthSound::GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, SoundInstance* instanceGeneric) {
  SineSynthSoundInstance* instance = static_cast<SineSynthSoundInstance*>(instanceGeneric);
  if (frame < instance->duration) {
    instance->radians += instance->radstep; // * speed
    while (instance->radians > (2.0 * M_PI)) {
      instance->radians -= static_cast<float>(2.0 * M_PI);
    }

    float s = sinf(instance->radians);
    for (uint8 channel = 0; channel < channels; ++channel) {
      samples[channel] = s;
    }
    return channels;
  }
  return 0;
}
