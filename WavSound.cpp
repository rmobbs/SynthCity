#include "WavSound.h"
#include "logging.h"

#include <iostream>

#include "SDL_audio.h"

#include "SerializeImpl.h"

static constexpr float kSilenceThresholdIntro = 0.01f;
static constexpr float kSilenceThresholdOutro = 0.50f;
static constexpr const char* kFileNameTag("filename");
static constexpr const char* kDecayTag("decay");

WavSound::WavSound() 
  : Sound("<fixme>") {

}

WavSound::WavSound(const std::string& soundName)
  : Sound(soundName) {

  // Fixme
  if (!LoadWav(soundName)) {
    throw std::runtime_error("Unable to load Wav file");
  }

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

bool WavSound::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.StartObject();

  // File tag:string
  w.Key(kFileNameTag);
  w.String(fileName.c_str());

  // Volume tag:string

  // Mute tag:boolean

  // Solo tag:boolean

  // Decay tag:string
  w.Key(kDecayTag);
  w.Double(decay);

  w.EndObject();

  return true;
}

bool WavSound::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  // File
  if (!d.HasMember(kFileNameTag) || !d[kFileNameTag].IsString()) {
    MCLOG(Warn, "Missing/invalid filename tag");
    return false;
  }

  if (!LoadWav(d[kFileNameTag].GetString())) {
    return false;
  }

  // Decay
  if (!d.HasMember(kDecayTag) || !d[kDecayTag].IsDouble()) {
    MCLOG(Warn, "Missing/invalid decay tag; decay will be 0");
    decay = 0.0f;
  }
  else {
    decay = d[kDecayTag].GetDouble();
  }

  return true;
}

bool WavSound::LoadWav(const std::string& fileName) {
  SDL_AudioSpec spec;
  uint8* data = nullptr;
  uint32 length = 0;

  if (SDL_LoadWAV(fileName.c_str(), &spec, &data, &length) == nullptr) {
    MCLOG(Error, "WavSound: SDL_LoadWAV failed for %s", fileName.c_str());
    return false;
  }

  // TODO: FIX THIS
  if (spec.channels != 1) {
    MCLOG(Error, "WavSound: %s has %d channels. Only mono "
      "WAV files are currently supported", fileName.c_str(), spec.channels);
    return false;
  }

  // TODO: Support more formats
  if (spec.format != AUDIO_S16SYS) {
    MCLOG(Error, "WavSound: %s has an unsupported format %d", fileName.c_str(), spec.format);
    return false;
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
    MCLOG(Warn, "WavSound: %s is entirely inaudible by "
      "the current metric - not trimming", fileName.c_str());
    audibleOffset = 0;
    audibleLength = length;
  }

  this->frequency = spec.freq;
  this->channels = spec.channels;
  this->data.assign(data + audibleOffset, data + audibleLength);

  SDL_FreeWAV(data);

  return true;
}

Voice* WavSound::CreateVoice() {
  Voice* voice = new Voice;
  voice->decay = decay * 0.0001f;
  return voice;
}