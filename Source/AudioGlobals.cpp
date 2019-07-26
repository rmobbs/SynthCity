#include "AudioGlobals.h"
#include "SDL_audio.h"

#include <limits>

namespace AudioGlobals {
  uint32 audioDeviceId = 0;

  void SetAudioDeviceId(uint32 audioDeviceId) {
    AudioGlobals::audioDeviceId = audioDeviceId;
  }

  uint32 GetAudioDeviceId() {
    return audioDeviceId;
  }

  void LockAudio() {
    SDL_LockAudioDevice(audioDeviceId);
  }

  void UnlockAudio() {
    SDL_UnlockAudioDevice(audioDeviceId);
  }
}
