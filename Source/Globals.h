#pragma once

#include "BaseTypes.h"

namespace Globals {
  static constexpr const char* kVersionTag("version");
  static constexpr const char* kVersionString("0.0.14");
  static constexpr const char* kMidiTags[] = { ".midi", ".mid" };
  static constexpr const char* kJsonTag(".json");
  static constexpr const char* kNameTag("name");
  static constexpr const char* kPathTag("path");
  static constexpr float kScrollBarWidth = 15.0f;
  static constexpr float kScrollBarHeight = 15.0f;
  static constexpr uint32 kDefaultMinNote = 8;
  static constexpr double kInverseDefaultMinNote = 1.0 / static_cast<double>(kDefaultMinNote);
  static constexpr uint32 kMinTempo = 40;
  static constexpr uint32 kDefaultTempo = 120;

  static constexpr float kHamburgerMenuWidth(20.0f);
  static constexpr const char* kTrackNameFormat("XXXXXXXXXXXXXXXXXXXXXXXX");
  static constexpr float kKeyboardKeyHeight = 20.0f;
  static constexpr const char* kDefaultNewTrackName("NewTrack");
  static constexpr uint32 kPlayTrackFlashColor = 0x00007F7F;
  static constexpr float kPlayTrackFlashDuration = 0.5f;
  static constexpr float kFullBeatWidth = 80.0f;
  static constexpr float kSequencerWindowToolbarHeight = 74.0f;

  extern uint32 mainWindowHandle;
  extern uint32 stopButtonTexture;

  extern double currentTime;
  extern double elapsedTime;

  extern bool vsyncEnabled;
}
