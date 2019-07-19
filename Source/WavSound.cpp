#include "WavSound.h"
#include "Logging.h"
#include "SerializeImpl.h"
#include "SoundFactory.h"
#include "resource.h"

#include "SDL_audio.h"
#include "imgui.h"

#include <iostream>
#include <atlbase.h>
#include <commdlg.h>
#include <filesystem>

static constexpr float kSilenceThresholdIntro = 0.01f;
static constexpr float kSilenceThresholdOutro = 0.50f;

REGISTER_SOUND(WavSound, "Sound from WAV file", DialogWavSound);
WavSound::WavSound(const std::string& fileName)
: Sound("WavSound") {

  if (!LoadWav(fileName)) {
    throw std::runtime_error("Unable to load WAV file");
  }

}

WavSound::WavSound(const ReadSerializer& serializer)
  : Sound("WavSound") {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Unable to serialize WAV sound");
  }
}

uint8 WavSound::GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, SoundInstance* instance) {
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

  std::string serializeFileName(fileName);

  if (serializer.rootPath.generic_string().length()) {
    std::string newFileName = std::filesystem::relative(serializeFileName, serializer.rootPath).generic_string();
    if (newFileName.length() > 0) {
      serializeFileName = newFileName;

      // Everything should work with the incorrect (on Windows) forward-slash paths
      // returned from std::filesystem functions, but for consistency we'll convert
      // the result to Windows-style backslashes
      std::replace(serializeFileName.begin(), serializeFileName.end(), '/', '\\');
    }
    else {
      MCLOG(Warn, "Instrument will reference absolute path for sound \'%s\'", serializeFileName.c_str());
    }
  }

  // File tag:string
  w.Key(kFileNameTag);
  w.String(serializeFileName.c_str());

  return true;
}

bool WavSound::SerializeRead(const ReadSerializer& serializer) {
  auto& r = serializer.d;

  // File
  if (!r.HasMember(kFileNameTag) || !r[kFileNameTag].IsString()) {
    MCLOG(Warn, "Missing/invalid filename tag");
    return false;
  }

  if (!LoadWav(r[kFileNameTag].GetString())) {
    return false;
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
    MCLOG(Error, "WavSound: %s has %r channels. Only mono "
      "WAV files are currently supported", fileName.c_str(), spec.channels);
    return false;
  }

  // TODO: Support more formats
  if (spec.format != AUDIO_S16SYS) {
    MCLOG(Error, "WavSound: %s has an unsupported format %r", fileName.c_str(), spec.format);
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

  this->fileName = fileName;

  return true;
}

SoundInstance* WavSound::CreateInstance() {
  SoundInstance* instance = new SoundInstance;
  instance->decay = decay * 0.0001f;
  return instance;
}

REGISTER_DIALOG(DialogWavSound);
bool DialogWavSound::Render() {
  char fileNameBuf[1024];
  strcpy_s(fileNameBuf, sizeof(fileNameBuf), fileName.c_str());
  if (ImGui::InputText("File", fileNameBuf, sizeof(fileNameBuf) - 1)) {
    fileName = std::string(fileNameBuf);
  }

  if (ImGui::Button("...")) {
    WCHAR szFile[FILENAME_MAX] = { 0 };
    OPENFILENAME ofn = { 0 };

    USES_CONVERSION;
    ofn.lStructSize = sizeof(ofn);

    ofn.lpstrTitle = A2W("Open WAV");
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = _TEXT("WAV\0*.wav\0");
    ofn.nFilterIndex = 0;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
      fileName = std::string(W2A(szFile));
    }
  }

  return true;
}

bool DialogWavSound::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.Key(WavSound::kFileNameTag);
  w.String(fileName.c_str());

  return true;
}

bool DialogWavSound::SerializeRead(const ReadSerializer& serializer) {
  return false;
}

