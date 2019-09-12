#pragma once

#include "BaseTypes.h"
#include <array>
#include <vector>

// Global input state (consumable)
class InputState {
public:
  static constexpr uint32 kMaxKey = 256;
  enum MouseButton {
    Left,
    Right,
    Count,
  };

  std::array<bool, MouseButton::Count> mouseButtonDown = { false };
  std::array<bool, MouseButton::Count> mouseButtonPress = { false };
  std::array<bool, MouseButton::Count> mouseButtonRelease = { false };

  int mouseX = 0;
  int mouseY = 0;
  int mouseScrollSign = 0;

  uint32 modState = 0;

  std::string inputText;
  std::array<uint8, kMaxKey> keyDown;
  std::array<uint8, kMaxKey> pressed;

  // Hardware key state tracker
  std::vector<bool> lastKeyboardState;

  static InputState& Get() {
    static InputState inputState;
    return inputState;
  }

  void BeginFrame();
  void SetFromKeyboardState();
};
