#include "Track.h"
#include "SoundFactory.h"
#include "Sequencer.h"
#include "Instrument.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Patch.h"
#include "imgui.h"

static constexpr std::string_view kDefaultNewTrackName("NewTrack");
static constexpr const char* kNameTag("name");
static constexpr const char* kColorSchemeTag("colorscheme");
static constexpr const char* kSoundsTag("sounds");
static constexpr uint32 kSubDialogPaddingSpacing = 5;

Track::Track() {

}

Track::Track(const ReadSerializer& serializer) {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Unable to serialize track");
  }
}

Track::~Track() {

}

void Track::AddNotes(uint32 noteCount, uint8 noteValue) {
  SDL_LockAudio();
  notes.resize(notes.size() + noteCount, noteValue);
  SDL_UnlockAudio();
}

void Track::SetNoteCount(uint32 noteCount, uint8 noteValue) {
  SDL_LockAudio();
  notes.resize(noteCount, noteValue);
  SDL_UnlockAudio();
}

void Track::SetNote(uint32 noteIndex, uint8 noteValue) {
  if (noteIndex >= notes.size()) {
    notes.resize(noteIndex + 1, 0);
  }
  notes[noteIndex] = noteValue;
}

void Track::ClearNotes() {
  SDL_LockAudio();
  std::fill(notes.begin(), notes.end(), 0);
  SDL_UnlockAudio();
}

bool Track::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.StartObject();

  // Name tag:string
  w.Key(kNameTag);
  w.String(name.c_str());

  // Color scheme tag:string
  if (colorScheme.length()) {
    w.Key(kColorSchemeTag);
    w.String(colorScheme.c_str());
  }

  patch->SerializeWrite(serializer);

  w.EndObject();

  return true;
}

bool Track::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  if (!d.HasMember(kNameTag) || !d[kNameTag].IsString()) {
    MCLOG(Error, "Invalid track in tracks array!");
    return false;
  }
  name = d[kNameTag].GetString();

  if (d.HasMember(kColorSchemeTag) && d[kColorSchemeTag].IsString()) {
    colorScheme = d[kColorSchemeTag].GetString();
  }

  try {
    patch = new Patch({ serializer });
  }
  catch (...) {
    return false;
  }

  return true;
}

DialogTrack::~DialogTrack() {
  delete subDialog;
  subDialog = nullptr;
}

void DialogTrack::Open() {
  // Pick an available name
  trackName = kDefaultNewTrackName;
  // Just feels weird and shameful to not have an upper bounds ...
  for (int nameSuffix = 1; nameSuffix < 1000; ++nameSuffix) {
    auto& tracks = Sequencer::Get().GetInstrument()->GetTracks();

    uint32 index;
    for (index = 0; index < tracks.size(); ++index) {
      if (tracks[index]->GetName() == trackName) {
        break;
      }
    }

    if (index >= tracks.size()) {
      break;
    }

    trackName = std::string(kDefaultNewTrackName) + std::to_string(nameSuffix);
  }

  soundName.clear();
  subDialog = nullptr;

  auto const& soundInfoMap = SoundFactory::GetInfoMap();
  if (!soundInfoMap.empty()) {
    // TODO: Setup a way to specify default
    soundName = soundInfoMap.begin()->first;

    auto const& dialogInfo = DialogFactory::GetInfoMap().find(soundInfoMap.begin()->second.dialog);
    if (dialogInfo != DialogFactory::GetInfoMap().end()) {
      subDialog = dialogInfo->second.factoryFunction();
    }
  }
  ImGui::OpenPopup("Add Track");
}

bool DialogTrack::Render() {
  ImGui::SetNextWindowSize(ImVec2(400.0f, 400.0f));

  bool isOpen = true;
  if (ImGui::BeginPopupModal("Add Track", &isOpen)) {
    // Track name
    char nameBuf[1024];
    strcpy_s(nameBuf, sizeof(nameBuf), trackName.c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
      trackName = std::string(nameBuf);
    }

    // Possible sound sources
    auto const& soundInfoMap = SoundFactory::GetInfoMap();
    if (!soundInfoMap.empty()) {
      if (ImGui::BeginCombo("Source", soundName.c_str())) {
        for (auto const& soundInfo : soundInfoMap) {
          ImGui::PushID(reinterpret_cast<const void*>(&soundInfo.second));
          if (ImGui::Selectable(soundInfo.first.c_str(), soundInfo.first == soundName)) {
            soundName = soundInfo.first;

            // Get the sub-dialog
            delete subDialog;

            auto const& dialogInfo = DialogFactory::GetInfoMap().find(soundInfo.second.dialog);
            if (dialogInfo != DialogFactory::GetInfoMap().end()) {
              subDialog = dialogInfo->second.factoryFunction();
            }
            else {
              subDialog = nullptr;
            }

          }
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
    }

    for (int i = 0; i < kSubDialogPaddingSpacing; ++i) {
      ImGui::Spacing();
    }

    if (subDialog != nullptr) {
      subDialog->Render();
    }

    for (int i = 0; i < kSubDialogPaddingSpacing; ++i) {
      ImGui::Spacing();
    }

    if (ImGui::Button("OK")) {
      exitedOk = true;

      rapidjson::StringBuffer sb;
      rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);

      // Write dialog into JSON buffer
      SerializeWrite({ w });

      // Read it back into a JSON document
      rapidjson::Document d;
      d.Parse(sb.GetString());

      // TODO: This is only handling track creation, not editing
      try {
        Sequencer::Get().GetInstrument()->AddTrack(new Track({ d }));
      }
      catch (...) {

      }

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

  return true;
}

bool DialogTrack::SerializeRead(const ReadSerializer& serializer) {
  return true;
}


