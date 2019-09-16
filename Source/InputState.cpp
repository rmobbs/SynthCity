#include "InputState.h"
#include "SDL.h"

void InputState::BeginFrame() {
  inputText.clear();

  std::fill(pressed.begin(), pressed.end(), false);
  std::fill(mouseButtonPress.begin(), mouseButtonPress.end(), false);
  std::fill(mouseButtonRelease.begin(), mouseButtonRelease.end(), false);

  mouseScrollSign = 0;
}

int32 InputState::GetFirstPressedKey() {
  int32 sdlKeyboardCount = 0;
  auto sdlKeyboardState = SDL_GetKeyboardState(&sdlKeyboardCount);
  for (int32 keyIndex = 0; keyIndex < sdlKeyboardCount; ++keyIndex) {
    if (sdlKeyboardState[keyIndex]) {
      auto keyCode = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(keyIndex));
      if (keyCode < InputState::kMaxKey) {
        return keyCode;
      }
    }
  }
  return -1;
}

void InputState::SetFromKeyboardState() {
  std::fill(pressed.begin(), pressed.end(), false);
  std::fill(released.begin(), released.end(), false);

  int32 sdlKeyboardCount = 0;
  auto sdlKeyboardState = SDL_GetKeyboardState(&sdlKeyboardCount);
  if (lastKeyboardState.size() < static_cast<size_t>(sdlKeyboardCount)) {
    lastKeyboardState.resize(sdlKeyboardCount);
  }
  if (sdlKeyboardState) {
    for (int32 k = 0; k < sdlKeyboardCount; ++k) {
      auto keyCode = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(k));

      if (keyCode < InputState::kMaxKey) {
        if (sdlKeyboardState[k]) {
          keyDown[keyCode] = true;

          if (!lastKeyboardState[k]) {
            pressed[keyCode] = true;
          }
        }
        else {
          keyDown[keyCode] = false;

          if (lastKeyboardState[k]) {
            released[keyCode] = true;
          }
        }

        lastKeyboardState[k] = sdlKeyboardState[k];
      }
    }
  }
}
