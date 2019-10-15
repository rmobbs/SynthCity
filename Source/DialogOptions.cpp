#include "DialogOptions.h"
#include "Globals.h"
#include "SDL.h"
#include "imgui.h"

static constexpr const char* kTitle("Options");
static constexpr float kMinDialogWidth(400.0f);
static constexpr float kMinDialogHeight(400.0f);

void DialogOptions::Open() {
  ImGui::OpenPopup(kTitle);
}

bool DialogOptions::Render() {
  ImGui::SetNextWindowSizeConstraints(ImVec2(kMinDialogWidth, kMinDialogHeight), ImVec2(1.0e9f, 1.0e9f));

  bool isOpen = true;
  if (ImGui::BeginPopupModal(kTitle, &isOpen)) {
    if (ImGui::Checkbox("Vertical Sync", &Globals::vsyncEnabled)) {
      SDL_GL_SetSwapInterval(Globals::vsyncEnabled ? 1 : 0);
    }

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetItemsLineHeightWithSpacing() * 1.2f);

    if (ImGui::Button("OK")) {
      exitedOk = true;
      ImGui::CloseCurrentPopup();
      isOpen = false;
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
      isOpen = false;
    }

    ImGui::EndPopup();
  }
  return isOpen;
}
