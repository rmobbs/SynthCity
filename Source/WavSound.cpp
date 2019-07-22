#include "WavSound.h"
#include "Logging.h"
#include "SerializeImpl.h"
#include "SoundFactory.h"
#include "WavBank.h"
#include "OddsAndEnds.h"
#include "resource.h"

#include "SDL_audio.h"
#include "imgui.h"

#include <iostream>
#include <atlbase.h>
#include <commdlg.h>
#include <filesystem>
#include <algorithm>

REGISTER_SOUND(WavSound, "Sound from WAV file");
WavSound::WavSound()
  : Sound("WavSound") {

}

WavSound::WavSound(const WavSound& that)
  : Sound(that)
  , wavData(that.wavData) {

}

WavSound::WavSound(const std::string& fileName)
: Sound("WavSound") {
  wavData = WavBank::Get().GetWav(fileName);
  if (!wavData) {
    throw std::runtime_error("Unable to load WAV file");
  }
}

WavSound::WavSound(const ReadSerializer& serializer)
  : Sound("WavSound") {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Unable to serialize WAV sound");
  }
}

void WavSound::RenderDialog() {
  std::string oldFileName;
  if (wavData != nullptr) {
    oldFileName = wavData->fileName;
  }

  char fileNameBuf[1024];
  strcpy_s(fileNameBuf, sizeof(fileNameBuf), oldFileName.c_str());

  std::string wavSoundUniqueId = std::string("WavSoundUniqueId") +
    std::to_string(reinterpret_cast<uint32>(&wavData));
  ImGui::PushID(wavSoundUniqueId.c_str());
  if (ImGui::InputText("File", fileNameBuf, sizeof(fileNameBuf) - 1)) {
    std::string newFileName(std::filesystem::absolute(fileNameBuf).generic_string());
    std::replace(newFileName.begin(), newFileName.end(), '/', '\\');

    if (!iequals(newFileName, oldFileName)) {
      SetWavData(WavBank::Get().GetWav(newFileName));
    }
  }
  ImGui::PopID();

  ImGui::PushID(wavSoundUniqueId.c_str());
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
      std::string newFileName(std::filesystem::absolute(W2A(szFile)).generic_string());
      std::replace(newFileName.begin(), newFileName.end(), '/', '\\');

      if (!iequals(newFileName, oldFileName)) {
        SetWavData(WavBank::Get().GetWav(newFileName));
      }
    }
  }
  ImGui::PopID();
}

uint8 WavSound::GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, SoundInstance* instance) {
  // Could eventually use the sound state for ADSR ...

  const uint32 frameSize = sizeof(int16) * wavData->channels;

  // Recall that the data buffer is uint8s, so to get the number of frames, divide its size
  // by the size of our frame
  if (frame >= wavData->data.size() / frameSize) {
    return 0;
  }

  for (uint8 channel = 0; channel < channels; ++channel) {
    samples[channel] = static_cast<float>(reinterpret_cast<int16*>(wavData->
      data.data() + frame * frameSize)[channel % wavData->channels]) / static_cast<float>(SHRT_MAX);
  }

  return channels;
}

bool WavSound::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  std::string serializeFileName(wavData->fileName);

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

  wavData = WavBank::Get().GetWav(r[kFileNameTag].GetString());
  if (!wavData) {
    return false;
  }

  return true;
}

Sound* WavSound::Clone() {
  return new WavSound(*this);
}

SoundInstance* WavSound::CreateInstance() {
  return new SoundInstance(this);
}

void WavSound::SetWavData(WavData* newWavData) {
  // TODO: ref count
  SDL_LockAudio();
  // Note: will not stop any playing voice
  wavData = newWavData;
  SDL_UnlockAudio();
}

