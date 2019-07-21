#include "DialogTrack.h"
#include "SerializeImpl.h"
#include "Track.h"
#include "Patch.h"
#include "Process.h"
#include "Sound.h"
#include "imgui.h"

DialogTrack::DialogTrack(Track* track)
  : track(track) {

}

DialogTrack::~DialogTrack() {

}

void DialogTrack::Open() {
  ImGui::OpenPopup("Add Track");
}

bool DialogTrack::Render() {
  ImGui::SetNextWindowSize(ImVec2(400.0f, 400.0f));

  bool isOpen = true;
  if (ImGui::BeginPopupModal("Add Track", &isOpen)) {
    // Track name
    char nameBuf[1024];
    strcpy_s(nameBuf, sizeof(nameBuf), track->GetName().c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
      track->SetName(std::string(nameBuf));
    }

    track->GetPatch()->RenderDialog();

    if (ImGui::Button("OK")) {
      exitedOk = true;

      rapidjson::StringBuffer sb;
      rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);

      // Write dialog into JSON buffer
      SerializeWrite({ w });

      // Read it back into a JSON document
      rapidjson::Document d;
      d.Parse(sb.GetString());

      /*
      // TODO: This is only handling track creation, not editing
      try {
        Sequencer::Get().GetInstrument()->AddTrack(new Track({ d }));
      }
      catch (...) {

      }
      */

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

bool DialogTrack::SerializeWrite(const WriteSerializer& serializer) {
#if 0
  auto& w = serializer.w;

  w.StartObject();

  // Name tag:string
  w.Key(kNameTag);
  w.String(trackName.c_str());

  w.Key(kSoundsTag);
  w.StartArray();

  w.StartObject();

  // Class tag:string
  w.Key(kClassTag);
  w.String(soundName.c_str());

  // Write out sub-dialog details into JSON buffer
  subDialog->SerializeWrite(serializer);

  w.EndObject();

  w.EndArray();
  w.EndObject();
#endif
  return true;
}

bool DialogTrack::SerializeRead(const ReadSerializer& serializer) {
  return true;
}


