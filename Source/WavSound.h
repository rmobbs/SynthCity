#pragma once

#include "Sound.h"
#include "DialogPage.h"
#include <vector>

class WavSound : public Sound {
public:
  static constexpr const char* kFileNameTag = "filename";
  static constexpr const char* kDecayTag = "decay";
protected:
  std::string fileName;
  uint32 frequency = 0;
  uint32 channels = 0;
  float decay = 0.0f;
  std::vector<uint8> data;

  bool LoadWav(const std::string& fileName);

public:
  WavSound(const std::string& fileName);
  WavSound(const ReadSerializer& serializer);

  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voice) override;

  uint32 getFrequency() const {
    return frequency;
  }

  Voice* CreateVoice() override;
  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

class DialogPageWavSound : public DialogPage {
public:
  DialogPageWavSound(HINSTANCE hInstance, HWND hWndParent);

  bool DialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

