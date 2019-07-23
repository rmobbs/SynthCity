#include "DialogTrack.h"
#include "SerializeImpl.h"
#include "Track.h"
#include "Patch.h"
#include "Process.h"
#include "Sound.h"
#include "Instrument.h"
#include "Mixer.h"
#include "imgui.h"
#include "imgui_internal.h"

static constexpr float kMinDialogWidth(600.0f);
static constexpr float kMinDialogHeight(600.0f);

DialogTrack::DialogTrack(Instrument* instrument, int32 trackIndex, Track* track, uint32 playButtonTexture, uint32 stopButtonTexture)
  : instrument(instrument)
  , trackIndex(trackIndex)
  , track(track)
  , playButtonTexture(playButtonTexture)
  , stopButtonTexture(stopButtonTexture) {

}

DialogTrack::~DialogTrack() {

}

void DialogTrack::Open() {
  ImGui::OpenPopup("Add Track");
}

bool DialogTrack::Render() {
  track->GetPatch()->UpdateDuration();

  ImGui::SetNextWindowSizeConstraints(ImVec2(kMinDialogWidth, kMinDialogHeight), ImVec2(1.0e9f, 1.0e9f));
  bool isOpen = true;
  if (ImGui::BeginPopupModal("Add Track", &isOpen)) {
    // Track name
    char nameBuf[1024];
    strcpy_s(nameBuf, sizeof(nameBuf), track->GetName().c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
      track->SetName(std::string(nameBuf));
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 60);

    auto& imGuiStyle = ImGui::GetStyle();

    auto oldItemSpacing = imGuiStyle.ItemSpacing;
    imGuiStyle.ItemSpacing.x = 2;

    if (ImGui::ArrowButtonEx("PlayButton", ImGuiDir_Right, ImVec2(22, 20), 0)) {
      if (playingVoiceId != -1) {
        Mixer::Get().StopVoice(playingVoiceId);
      }
      playingVoiceId = Mixer::Get().PlayPatch(track->GetPatch(), 1.0f);
    }

    ImGui::SameLine();

    if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(stopButtonTexture), ImVec2(14, 14))) {
      if (playingVoiceId != -1) {
        Mixer::Get().StopVoice(playingVoiceId);
        playingVoiceId = -1;
      }
    }

    imGuiStyle.ItemSpacing = oldItemSpacing;

    track->GetPatch()->RenderDialog();

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

  if (!isOpen) {
    if (playingVoiceId != -1) {
      Mixer::Get().StopVoice(playingVoiceId);
      playingVoiceId = -1;
    }

    if (exitedOk) {
      if (trackIndex != -1) {
        instrument->ReplaceTrack(trackIndex, track);
      }
      else {
        instrument->AddTrack(track);
      }
    }
    else {
      delete track;
    }
  }
  return isOpen;
}

