#include "DialogTrack.h"
#include "SerializeImpl.h"
#include "Track.h"
#include "Patch.h"
#include "Process.h"
#include "Sound.h"
#include "Sequencer.h"
#include "Instrument.h"
#include "Song.h"
#include "imgui.h"
#include "imgui_internal.h"

static constexpr float kMinDialogWidth(600.0f);
static constexpr float kMinDialogHeight(730.0f);

DialogTrack::DialogTrack(std::string title, Instrument* instrument, uint32 replaceTrackId, Track* track)
  : title(title)
  , instrument(instrument)
  , replaceTrackId(replaceTrackId)
  , track(track) {

}

DialogTrack::~DialogTrack() {
  delete track;
  track = nullptr;
}

void DialogTrack::Open() {
  wasPlaying = Sequencer::Get().IsPlaying();
  Sequencer::Get().PauseKill();
  ImGui::OpenPopup(title.c_str());
}

bool DialogTrack::Render() {
  track->GetPatch()->UpdateDuration();

  ImGui::SetNextWindowSizeConstraints(ImVec2(kMinDialogWidth, kMinDialogHeight), ImVec2(1.0e9f, 1.0e9f));
  bool isOpen = true;
  if (ImGui::BeginPopupModal(title.c_str(), &isOpen)) {
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
        Sequencer::Get().StopVoice(playingVoiceId);
      }
      playingVoiceId = Sequencer::Get().PlayPatch(track->GetPatch(), 1.0f);
    }

    ImGui::SameLine();

    if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(Globals::stopButtonTexture), ImVec2(14, 14))) {
      if (playingVoiceId != -1) {
        Sequencer::Get().StopVoice(playingVoiceId);
        playingVoiceId = -1;
      }
    }

    imGuiStyle.ItemSpacing = oldItemSpacing;

    // Volume slider
    float volume = track->GetVolume();
    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
      track->SetVolume(volume);
    }

    // Instrument palette key
    auto& trackPaletteKey = track->GetColorScheme();
    if (ImGui::BeginCombo("Palette Key", trackPaletteKey.empty() ? "<None>" : trackPaletteKey.c_str())) {
      auto& instrumentPalette = instrument->GetTrackPalette();
      for (auto& paletteKey : instrumentPalette) {
        if (ImGui::Selectable(paletteKey.first.c_str(), paletteKey.first == trackPaletteKey)) {
          track->SetColorScheme(paletteKey.first);
        }
      }
      ImGui::EndCombo();
    }

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

  return isOpen;
}

void DialogTrack::Close() {
  if (playingVoiceId != -1) {
    Sequencer::Get().StopVoice(playingVoiceId);
    playingVoiceId = -1;
  }

  if (exitedOk) {
    if (replaceTrackId != kInvalidUint32) {
      instrument->ReplaceTrackById(replaceTrackId, track);
    }
    else {
      instrument->AddTrack(track);

      // Force-update the instances
      auto song = Sequencer::Get().GetSong();
      if (song != nullptr) {
        song->AddMeasures(0);
      }
    }
  }
  else {
    delete track;
  }

  track = nullptr;

  if (wasPlaying) {
    Sequencer::Get().Play();
    wasPlaying = false;
  }
}

