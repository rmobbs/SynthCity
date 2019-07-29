#include "ImGuiExtensions.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
namespace ImGui {
  void FillRect(ImVec2 extents, ImU32 color) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
      return;

    const ImRect check_bb(window->DC.CursorPos, window->DC.CursorPos + extents);
    window->DrawList->AddRectFilled(check_bb.GetTL(), check_bb.GetBR(), color, 0);
  }

  void DrawRect(ImVec2 extents, ImU32 color) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
      return;

    const ImRect check_bb(window->DC.CursorPos, window->DC.CursorPos + extents);
    window->DrawList->AddRect(check_bb.GetTL(), check_bb.GetBR(), color, 0);
  }

  bool SquareRadioButton(const char* label, bool active, float w, float h) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
      return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImRect check_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, h));
    ItemSize(check_bb, 0);

    if (!ItemAdd(check_bb, id))
      return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(check_bb, id, &hovered, &held);
    RenderNavHighlight(check_bb, id);
    if (active)
    {
      window->DrawList->AddRectFilled(check_bb.GetTL(), check_bb.GetBR(), GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 0);
    }
    window->DrawList->AddRect(check_bb.GetTL(), check_bb.GetBR(), GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 0);

    if (g.LogEnabled)
      LogRenderedText(&check_bb.Min, active ? "(x)" : "( )");
    return pressed;
  }
};
