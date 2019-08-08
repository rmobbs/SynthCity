#include "WavBank.h"
#include "Logging.h"

#include "SDL_audio.h"

#include <filesystem>

static constexpr float kSilenceThresholdIntro = 0.01f;

/* static */
WavBank* WavBank::singleton = nullptr;

bool WavBank::InitSingleton() {
  if (!singleton) {
    singleton = new WavBank;
    return true;
  }
  return false;
}

bool WavBank::TermSingleton() {
  delete singleton;
  return true;
}

WavBank::~WavBank() {
  for (auto wavMapEntry : wavDataMap) {
    delete wavMapEntry.second;
  }
  wavDataMap.clear();
}

WavData* WavBank::GetWav(std::string const& fileName) {
  // Make it absolute
  auto absoluteName = std::filesystem::absolute(fileName).generic_string();

  // Everything should work with the incorrect (on Windows) forward-slash paths
  // returned from std::filesystem functions, but for consistency we'll convert
  // the result to Windows-style backslashes
  std::replace(absoluteName.begin(), absoluteName.end(), '/', '\\');

  // Check the map
  auto wavMapEntry = wavDataMap.find(absoluteName);
  if (wavMapEntry != wavDataMap.end()) {
    return wavMapEntry->second;
  }

  // Load
  auto wavData = LoadWav(absoluteName);
  if (wavData != nullptr) {
    wavDataMap.insert(std::pair<std::string, WavData*>(absoluteName, wavData));
  }
  return wavData;
}

WavData* WavBank::LoadWav(std::string const& fileName) {
  SDL_AudioSpec spec;
  uint8* data = nullptr;
  uint32 length = 0;

  if (SDL_LoadWAV(fileName.c_str(), &spec, &data, &length) == nullptr) {
    MCLOG(Error, "WavBank: SDL_LoadWAV failed for %s", fileName.c_str());
    return false;
  }

  // TODO: FIX THIS
  if (spec.channels != 1) {
    MCLOG(Error, "WavBank: %s has %d channels. Only mono "
      "WAV files are currently supported", fileName.c_str(), spec.channels);
    return false;
  }

  // TODO: Support more formats
  if (spec.format != AUDIO_S16SYS) {
    MCLOG(Error, "WavBank: %s has an unsupported format %d", fileName.c_str(), spec.format);
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
    MCLOG(Warn, "WavBank: %s is entirely inaudible by "
      "the current metric - not trimming", fileName.c_str());
    audibleOffset = 0;
    audibleLength = length;
  }

  auto wavData = new WavData;

  wavData->channels = spec.channels;
  wavData->fileName = fileName;
  wavData->duration = (static_cast<float>(audibleLength) /
    static_cast<float>(frameSize)) / static_cast<float>(spec.freq);
  wavData->data.assign(data + audibleOffset, data + audibleLength);

  SDL_FreeWAV(data);

  return wavData;
}

