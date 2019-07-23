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

SynthSound::SynthSound(const std::string& className)
  : Sound(className) {

}

SynthSound::SynthSound(const std::string& className, uint32 frequency, uint32 durationNum, uint32 durationDen)
  : Sound(className)
  , frequency(frequency) {

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

// Sine
REGISTER_SOUND(SineSynthSound, "Modulated sine wave");
SineSynthSound::SineSynthSound()
  : SynthSound("SineSynthSound") {

}

SineSynthSound::SineSynthSound(const SineSynthSound& that)
  : SynthSound(that) {

}

SineSynthSound::SineSynthSound(uint32 frequency, uint32 durationNum, uint32 durationDen)
  : SynthSound("SineSynthSound", frequency, durationNum, durationDen) {
}

SineSynthSound::SineSynthSound(const ReadSerializer& serializer)
  : SynthSound("SineSynthSound", serializer) {
}

Sound* SineSynthSound::Clone() {
  return new SineSynthSound(*this);
}

SoundInstance* SineSynthSound::CreateInstance() {
  SineSynthSoundInstance* instance = new SineSynthSoundInstance(this);

  instance->radstep = static_cast<float>((2.0 * M_PI *
    frequency) / static_cast<double>(Mixer::kDefaultFrequency));
  return instance;
}

uint8 SineSynthSound::GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, SoundInstance* instanceGeneric) {
  SineSynthSoundInstance* instance = static_cast<SineSynthSoundInstance*>(instanceGeneric);
  if (frame < static_cast<uint32>(duration * 44100.0f)) {
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

void SineSynthSound::RenderDialog() {
  int frequencyInt = frequency;
  if (ImGui::InputInt("Frequency", &frequencyInt)) {
    if (frequencyInt < 1000) {
      frequencyInt = 1000;
    }
    frequency = frequencyInt;
  }

  float durationTmp = duration;
  if (ImGui::InputFloat("Duration", &durationTmp)) {
    if (durationTmp < std::numeric_limits<float>::epsilon()) {
      durationTmp = std::numeric_limits<float>::epsilon();
    }
    duration = durationTmp;
  }
}
