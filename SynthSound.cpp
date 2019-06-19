#include "SynthSound.h"
#define _USE_MATH_DEFINES
#include <math.h>

REGISTER_SYNTH_SOUND({ "SinusSynthSound", "Sine wave" });
SinusSynthSound::SinusSynthSound(uint32 samplerFrequency, uint32 frequency, uint32 duration)
  : SynthSound("SineSynthSound", samplerFrequency, frequency, duration) {
}

Voice* SinusSynthSound::CreateVoice() {
  SinusSynthVoice* voice = new SinusSynthVoice;
  voice->radstep = static_cast<float>((2.0 * M_PI * frequency) / static_cast<double>(samplerFrequency));
  return voice;
}

uint8 SinusSynthSound::GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voiceGeneric) {
  if (frame < beatLength) {
    SinusSynthVoice* voice = static_cast<SinusSynthVoice*>(voiceGeneric);

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
