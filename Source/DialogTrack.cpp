#include "DialogTrack.h"
#include "SerializeImpl.h"
#include "Track.h"
#include "Patch.h"
#include "Process.h"
#include "Sound.h"
#include "Instrument.h"
#include "Mixer.h"
#include "imgui.h"

static constexpr float kDialogWidth(600.0f);
static constexpr float kDialogHeight(640.0f);

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
  ImGui::SetNextWindowSize(ImVec2(kDialogWidth, kDialogHeight));

  bool isOpen = true;
  if (ImGui::BeginPopupModal("Add Track", &isOpen)) {
    // Track name
    char nameBuf[1024];
    strcpy_s(nameBuf, sizeof(nameBuf), track->GetName().c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
      track->SetName(std::string(nameBuf));
    }

    if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(playButtonTexture), ImVec2(20, 20))) {
      if (playingVoiceId != -1) {
        Mixer::Get().StopVoice(playingVoiceId);
      }
      playingVoiceId = Mixer::Get().PlayPatch(track->GetPatch(), 1.0f);
    }

    ImGui::SameLine();

    if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(stopButtonTexture), ImVec2(20, 20))) {
      if (playingVoiceId != -1) {
        Mixer::Get().StopVoice(playingVoiceId);
        playingVoiceId = -1;
      }
    }

    track->GetPatch()->RenderDialog();

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

