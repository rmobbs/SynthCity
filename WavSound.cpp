#include "WavSound.h"
#include "logging.h"

#include <iostream>

#include "SDL_audio.h"

static constexpr float kSilenceThresholdIntro = 0.01f;
static constexpr float kSilenceThresholdOutro = 0.50f;

WavSound::WavSound(const std::string& soundName)
  : Sound(soundName) {

  SDL_AudioSpec spec;
  uint8* data = nullptr;
  uint32 length = 0;

  if (SDL_LoadWAV(soundName.c_str(), &spec, &data, &length) == nullptr) {
    std::string strError = "WavSound: SDL_LoadWAV failed for " + soundName;
    MCLOG(Error, strError.c_str());
    throw std::runtime_error(strError);
  }

  // TODO: FIX THIS
  if (spec.channels != 1) {
    std::string strError = "WavSound: " + soundName + " has " +
      std::to_string(spec.channels) + " channels. Only mono WAV files are currently supported";
    MCLOG(Error, strError.c_str());
    throw std::runtime_error(strError);
  }

  // TODO: Support more formats
  if (spec.format != AUDIO_S16SYS) {
    std::string strError = "WavSound: " + soundName +
      " has an unsupported format " + std::to_string(spec.format);
    MCLOG(Error, strError.c_str());
    throw std::runtime_error(strError);
  }

  // Skip any inaudible intro ...
  // TODO: this should really be a preprocessing step
  auto frameSize = sizeof(uint16) * spec.channels;
  auto audibleOffset = 0;
  auto audibleLength = length;

  while (audibleLength > 0) {
    int32 sum = 0;
    for (int c = 0; c < spec.channels; ++c) {
      sum += reinterpret_cast<const int16*>(data + audibleOffset)[c];
    }

    if ((static_cast<float>(sum) / SHRT_MAX) > kSilenceThresholdIntro) {
      break;
    }

    audibleOffset += frameSize;
    audibleLength -= frameSize;
  }

  if (audibleLength <= 0) {
    MCLOG(Warn, "WavSound: %s is entirely inaudible by the current metric - not trimming", soundName.c_str());
    audibleOffset = 0;
    audibleLength = length;
  }

  this->frequency = spec.freq;
  this->channels = spec.channels;
  this->data.assign(data + audibleOffset, data + audibleLength);

  SDL_FreeWAV(data);
}

uint8 WavSound::GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voice) {
  // Could eventually use the sound state for ADSR ...

  const uint32 frameSize = sizeof(int16) * this->channels;

  // Recall that the data buffer is uint8s, so to get the number of frames, divide its size
  // by the size of our frame
  if (frame >= this->data.size() / frameSize) {
    return 0;
  }

  for (uint8 channel = 0; channel < channels; ++channel) {
    samples[channel] = static_cast<float>(reinterpret_cast<int16*>(this->
      data.data() + frame * frameSize)[channel % this->channels]) / static_cast<float>(SHRT_MAX);
  }

  return channels;
}
