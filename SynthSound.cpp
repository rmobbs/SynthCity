#include "SynthSound.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Mixer.h"
#include "SoundFactory.h"
#include "Sequencer.h"
#include "resource.h"
#include <algorithm>

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
  w.Uint(duration);

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
  duration = r[kDurationTag].GetUint();

  return true;
}

DialogPageSynthSound::DialogPageSynthSound(HINSTANCE hInstance, HWND hWndParent)
  : DialogPage(hInstance, hWndParent, IDD_TRACKPROPERTIES_SYNTH) {

}

bool DialogPageSynthSound::DialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_USERINIT: {
      // Set the defaults
      SetDlgItemInt(hWndDlg, IDC_EDIT_SYNTHPROPERTIES_FREQUENCY, SynthSound::kDefaultFrequency, FALSE);
      SetDlgItemInt(hWndDlg, IDC_EDIT_SYNTHPROPERTIES_DURATION_BAR, SynthSound::kDefaultBeatsPerBar, FALSE);
      SetDlgItemInt(hWndDlg, IDC_EDIT_SYNTHPROPERTIES_DURATION_BEAT, SynthSound::kDefaultNotesPerBeat, FALSE);
      break;
    }
  }
  return false;
}

bool DialogPageSynthSound::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  // Frequency
  uint32 frequency =
    GetDlgItemInt(GetHandle(), IDC_EDIT_SYNTHPROPERTIES_FREQUENCY, nullptr, FALSE);
  w.Key("frequency");
  w.Uint(frequency);

  // Duration
  INT num = GetDlgItemInt(GetHandle(), IDC_EDIT_SYNTHPROPERTIES_DURATION_BAR, nullptr, FALSE);
  INT den = GetDlgItemInt(GetHandle(), IDC_EDIT_SYNTHPROPERTIES_DURATION_BEAT, nullptr, FALSE);
  den = std::min(std::max(static_cast<uint32>(den), 1u), Sequencer::Get().GetMaxSubdivisions());
  uint32 duration = static_cast<uint32>((static_cast<float>(num) /
    static_cast<float>(den)) * (Mixer::kDefaultFrequency / Mixer::kDefaultChannels));
  w.Key("duration");
  w.Uint(duration);

  return true;
}

bool DialogPageSynthSound::SerializeRead(const ReadSerializer& serializer) {
  return false;
}


// Sine
REGISTER_SOUND(SineSynthSound, "Modulated sine wave", DialogPageSynthSound);
SineSynthSound::SineSynthSound(uint32 frequency, uint32 duration)
  : SynthSound("SineSynthSound", frequency, duration) {
}

SineSynthSound::SineSynthSound(const ReadSerializer& serializer)
  : SynthSound("SineSynthSound", serializer) {
}

Voice* SineSynthSound::CreateVoice() {
  SineSynthVoice* voice = new SineSynthVoice;
  voice->radstep = static_cast<float>((2.0 * M_PI *
    frequency) / static_cast<double>(Mixer::kDefaultFrequency));
  return voice;
}

uint8 SineSynthSound::GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voiceGeneric) {
  if (frame < duration) {
    SineSynthVoice* voice = static_cast<SineSynthVoice*>(voiceGeneric);

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
