#pragma once

#include "BaseTypes.h"

namespace AudioGlobals {
  void SetAudioDeviceId(uint32 audioDeviceId);
  void LockAudio();
  void UnlockAudio();
  uint32 GetAudioDeviceId();
}