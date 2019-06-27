#pragma once

#include "Sound.h"
#include "DialogPage.h"

class SynthSound : public Sound {
public:
  static constexpr uint32 kDefaultFrequency = 1000; // MHz
  static constexpr uint32 kDefaultBeatsPerBar = 1; // Numerator of time sig
  static constexpr uint32 kDefaultNotesPerBeat = 4; // Denominator of time sig
protected:
  uint32 frequency = 0;
  uint32 duration = 0;
public:
  SynthSound(const std::string& className, uint32 frequency, uint32 duration)
    : Sound(className)
    , frequency(frequency)
    , duration(duration) {

  }

  SynthSound(std::string className, const ReadSerializer& serializer);

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

class DialogPageSynthSound : public DialogPage {
public:
  bool DialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

  DialogPageSynthSound(HINSTANCE hInstance, HWND hWndParent);

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

class SineSynthVoice : public Voice {
public:
  float radians = 0;
  float radstep = 0;
};

class SineSynthSound : public SynthSound {
public:
  SineSynthSound(uint32 frequency, uint32 duration);
  SineSynthSound(const ReadSerializer& serializer);

  Voice* CreateVoice() override;
  uint8 GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voice) override;
};
