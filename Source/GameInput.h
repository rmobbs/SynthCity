#pragma once

#include "GameGlobals.h"

#include <array>
#include <vector>

class GameInput {
private:
  std::array<uint32, GameGlobals::kNumGameplayLines> buttonKeys = { 0 };
  std::array<bool, GameGlobals::kNumGameplayLines> lastButtonState = { false };
public:
  GameInput(const std::array<uint32, GameGlobals::kNumGameplayLines>& buttonKeys);

  void TakeSnapshot(double beatTime, std::array<double, GameGlobals::kNumGameplayLines>& presses, std::array<double, GameGlobals::kNumGameplayLines>& releases);

  bool SetLineKey(uint32 line, uint32 key);

  inline uint32 GetLineKey(uint32 lineIndex) {
    return buttonKeys[lineIndex];
  }
};
