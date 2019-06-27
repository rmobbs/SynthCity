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
  w.Uint(durationNum << 16 | durationDen);

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
  uint32 durationMasked = r[kDurationTag].GetUint();

  durationNum = durationMasked >> 16;
  durationDen = durationMasked & 0xFFFF;

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

  // The time value of the note unit in which our duration is expressed, i.e. half-note, quarter-note
  // We'll make sure it's between [2,<upper-limit>] and it's even
  INT den = GetDlgItemInt(GetHandle(), IDC_EDIT_SYNTHPROPERTIES_DURATION_BEAT, nullptr, FALSE);
  den = std::min(std::max(static_cast<uint32>(den), 2u), Sequencer::Get().GetMaxSubdivisions()) & ~1u;

  // The number of these notes that defines the duration
  INT num = GetDlgItemInt(GetHandle(), IDC_EDIT_SYNTHPROPERTIES_DURATION_BAR, nullptr, FALSE);

  w.Key("duration");
  w.Uint(num << 16 | den);

  return true;
}

bool DialogPageSynthSound::SerializeRead(const ReadSerializer& serializer) {
  return false;
}


// Sine
REGISTER_SOUND(SineSynthSound, "Modulated sine wave", DialogPageSynthSound);
SineSynthSound::SineSynthSound(uint32 frequency, uint32 durationNum, uint32 durationDen)
  : SynthSound("SineSynthSound", frequency, durationNum, durationDen) {
}

SineSynthSound::SineSynthSound(const ReadSerializer& serializer)
  : SynthSound("SineSynthSound", serializer) {
}

Voice* SineSynthSound::CreateVoice() {
  SineSynthVoice* voice = new SineSynthVoice;

  float durationInSeconds = static_cast<float>(Sequencer::Get().
    GetSecondsPerBeat()) * (static_cast<float>(durationNum) / static_cast<float>(durationDen));

  voice->duration = static_cast<uint32>(durationInSeconds * static_cast<float>(Mixer::kDefaultFrequency));

  voice->radstep = static_cast<float>((2.0 * M_PI *
    frequency) / static_cast<double>(Mixer::kDefaultFrequency));
  return voice;
}

uint8 SineSynthSound::GetSamplesForFrame(float* samples, uint8 channels, uint32 frame, Voice* voiceGeneric) {
  SineSynthVoice* voice = static_cast<SineSynthVoice*>(voiceGeneric);
  if (frame < voice->duration) {
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
