#pragma once

#include "GameGlobals.h"

#include <array>
#include <atomic>
#include <vector>

class GameInput {
private:
  std::array<std::atomic<float>, GameGlobals::kNumGameplayLines> buttonPressTimes = { 0.0f };
  std::array<std::atomic<float>, GameGlobals::kNumGameplayLines> buttonReleaseTimes = { 0.0f };

  std::array<uint32, GameGlobals::kNumGameplayLines> buttonKeys = { 0 };
  std::array<bool, GameGlobals::kNumGameplayLines> lastButtonState = { false };
public:
  GameInput(const std::array<uint32, GameGlobals::kNumGameplayLines>& buttonKeys);

  void TakeSnapshot(float beatTime, std::array<float, GameGlobals::kNumGameplayLines>& presses, std::array<float, GameGlobals::kNumGameplayLines>& releases);

  bool ConsumePresses(std::array<float, GameGlobals::kNumGameplayLines>& outPresses);
  bool ConsumeReleases(std::array<float, GameGlobals::kNumGameplayLines>& outReleases);

  bool SetLineKey(uint32 line, uint32 key);

  inline uint32 GetLineKey(uint32 lineIndex) {
    return buttonKeys[lineIndex];
  }
};
