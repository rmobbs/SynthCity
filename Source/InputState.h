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
  int scroll = 0;
  std::string inputText;
  std::array<uint8, kMaxKey> wasDown;
  std::array<uint8, kMaxKey> keyDown;

  static InputState& get() {
    static InputState inputState;
    return inputState;
  }
};
