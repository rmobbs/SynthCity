#include "InputState.h"
void InputState::BeginFrame() {
  inputText.clear();

  std::fill(pressed.begin(), pressed.end(), false);
  std::fill(mouseButtonPress.begin(), mouseButtonPress.end(), false);
  std::fill(mouseButtonRelease.begin(), mouseButtonRelease.end(), false);

  mouseScrollSign = 0;
}
