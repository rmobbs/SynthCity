#pragma once

#include "BaseTypes.h"
#include <array>

// Global input state (consumable)
class InputState {
public:
  static constexpr uint32 kMaxKey = 256;

  int mouseX = 0;
  int mouseY = 0;
  bool downL = false;
  bool downR = false;
  int scroll = 0;
  uint32 modState = 0;
  std::string inputText;
  std::array<uint8, kMaxKey> wasDown;
  std::array<uint8, kMaxKey> keyDown;
  std::array<uint8, kMaxKey> pressed;

  static InputState& Get() {
    static InputState inputState;
    return inputState;
  }
};
