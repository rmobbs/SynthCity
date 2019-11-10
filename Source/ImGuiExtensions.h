#pragma once

#include "imgui.h"

namespace ImGui {
  void DrawRect(ImVec2 extents, ImU32 color);
  void FillRect(ImVec2 extents, ImU32 color);
  bool SquareRadioButton(const char* label, bool active, float w, float h);
  bool IsEditing();
  void InvisibleSeparator();
  void NewLine(float lineSize);
}