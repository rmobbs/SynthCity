#include "GameInput.h"
#include "SDL.h"
#include <assert.h>

GameInput::GameInput(const std::array<uint32, GameGlobals::kNumGameplayLines>& buttonKeys)
  : buttonKeys(buttonKeys) {

}

void GameInput::TakeSnapshot(double beatTime, std::array<double, GameGlobals::kNumGameplayLines>& presses, std::array<double, GameGlobals::kNumGameplayLines>& releases) {
  int32 sdlKeyboardCount = 0;
  auto sdlKeyboardState = SDL_GetKeyboardState(&sdlKeyboardCount);

  for (size_t keyIndex = 0; keyIndex < GameGlobals::kNumGameplayLines; ++keyIndex) {
    auto scanCode = SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(buttonKeys[keyIndex]));
    if (scanCode < sdlKeyboardCount) {
      if (sdlKeyboardState[scanCode]) {
        if (!lastButtonState[keyIndex]) {
          presses[keyIndex] = beatTime;
        }
      }
      else {
        if (lastButtonState[keyIndex]) {
          releases[keyIndex] = beatTime;
        }
      }
    }
    lastButtonState[keyIndex] = sdlKeyboardState[scanCode];
  }
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