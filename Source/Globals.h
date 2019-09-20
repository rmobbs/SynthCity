#pragma once

#include "BaseTypes.h"

namespace Globals {
  static constexpr const char* kVersionTag("version");
  static constexpr const char* kVersionString("0.0.9");
  static constexpr const char* kMidiTags[] = { ".midi", ".mid" };
  static constexpr const char* kJsonTag(".json");
  static constexpr const char* kNameTag("name");
  static constexpr const char* kPathTag("path");
  static constexpr float kScrollBarWidth = 15.0f;
  static constexpr float kScrollBarHeight = 15.0f;
  static constexpr uint32 kDefaultMinNote = 8;
  static constexpr uint32 kDefaultTempo = 120;

  extern double currentTime;
}
