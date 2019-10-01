#include "GameInput.h"
#include "SDL.h"
#include <assert.h>

GameInput::GameInput(const std::array<uint32, GameGlobals::kNumGameplayLines>& buttonKeys)
  : buttonKeys(buttonKeys) {

}

void GameInput::TakeSnapshot(float beatTime, std::array<float, GameGlobals::kNumGameplayLines>& presses, std::array<float, GameGlobals::kNumGameplayLines>& releases) {
  int32 sdlKeyboardCount = 0;
  auto sdlKeyboardState = SDL_GetKeyboardState(&sdlKeyboardCount);

  for (size_t keyIndex = 0; keyIndex < GameGlobals::kNumGameplayLines; ++keyIndex) {
    auto scanCode = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(buttonKeys[keyIndex]));
    if (scanCode < sdlKeyboardCount) {
      if (sdlKeyboardState[scanCode]) {
        if (!lastButtonState[keyIndex]) {
          presses[keyIndex] = beatTime;
          buttonPressTimes[keyIndex] = beatTime;
        }
      }
      else {
        if (lastButtonState[keyIndex]) {
          releases[keyIndex] = beatTime;
          buttonReleaseTimes[keyIndex] = beatTime;
        }
      }
    }
    lastButtonState[keyIndex] = sdlKeyboardState[scanCode];
  }
}

bool GameInput::ConsumePresses(std::array<float, GameGlobals::kNumGameplayLines>& outPresses) {
  bool anyPressed = false;
  for (size_t keyIndex = 0; keyIndex < GameGlobals::kNumGameplayLines; ++keyIndex) {
    anyPressed |= (outPresses[keyIndex] = buttonPressTimes[keyIndex].exchange(0.0f)) > 0.0f;
  }
  return anyPressed;
}

bool GameInput::ConsumeReleases(std::array<float, GameGlobals::kNumGameplayLines>& outReleases) {
  bool anyReleased = false;
  for (size_t keyIndex = 0; keyIndex < GameGlobals::kNumGameplayLines; ++keyIndex) {
    anyReleased |= (outReleases[keyIndex] = buttonReleaseTimes[keyIndex].exchange(0.0f)) > 0.0f;
  }
  return anyReleased;
}

bool GameInput::SetLineKey(uint32 line, uint32 key) {
  for (size_t keyIndex = 0; keyIndex < GameGlobals::kNumGameplayLines; ++keyIndex) {
    if (buttonKeys[keyIndex] == key && line != keyIndex) {
      return false;
    }
  }
  buttonKeys[line] = key;
  return true;
}