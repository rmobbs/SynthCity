#include "Patch.h"
#include "Logging.h"
#include "SerializeImpl.h"
#include "ProcessFactory.h"
#include "SoundFactory.h"
#include "ProcessDecay.h"
#include "Sound.h"
#include "Globals.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuiExtensions.h"

#include <stdexcept>

static constexpr const char* kPatchTag("patch");
static constexpr const char* kProcessesTag("processes");
static constexpr const char* kSoundsTag("sounds");
static constexpr uint32 kSubDialogPaddingSpacing = 5;

Patch::Patch(const Patch& that) {
  soundDuration = that.soundDuration;

  for (const auto& process : that.processes) {
    this->processes.push_back(process->Clone());
  }
  for (const auto& sound : that.sounds) {
    this->sounds.push_back(sound->Clone());
  }
}

Patch::Patch(const ReadSerializer& serializer) {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Unable to serialize patch");
  }
}

Patch::~Patch() {
  for (auto& process : processes) {
    delete process;
  }
  processes.clear();

  for (auto& sound : sounds) {
    delete sound;
  }
  sounds.clear();
}

bool Patch::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  if (!d.HasMember(kPatchTag) || !d[kPatchTag].IsObject()) {
    MCLOG(Error, "Missing or invalid patch section in track");
    return false;
  }

  const auto& patch = d[kPatchTag];

  // Processes
  if (!patch.HasMember(kProcessesTag) || !patch[kProcessesTag].IsArray() || !patch[kProcessesTag].Size()) {
    MCLOG(Error, "Missing or invalid process array in patch");
    return false;
  }
  const auto& processesArray = patch[kProcessesTag];
  for (uint32 processIndex = 0; processIndex < processesArray.Size(); ++processIndex) {
    const auto& processEntry = processesArray[processIndex];

    // Get class
    if (!processEntry.HasMember(kClassTag) || !processEntry[kClassTag].IsString()) {
      MCLOG(Error, "No class tag for process");
      continue;
    }

    std::string className(processEntry[kClassTag].GetString());

    // Get factory
    const auto& processInfoMap = ProcessFactory::GetInfoMap();
    const auto& processInfo = processInfoMap.find(className);
    if (processInfo == processInfoMap.end()) {
      MCLOG(Error, "Invalid class tag for process");
      continue;
    }

    try {
      AddProcess(processInfo->second.serialize({ processEntry }));
    }
    catch (...) {
      continue;
    }
  }

  // Sounds
  if (!patch.HasMember(kSoundsTag) || !patch[kSoundsTag].IsArray() || !patch[kSoundsTag].Size()) {
    MCLOG(Error, "Missing or invalid sound array in patch");
    return false;
  }
  const auto& soundArray = patch[kSoundsTag];
  for (uint32 soundIndex = 0; soundIndex < soundArray.Size(); ++soundIndex) {
    // Take the first one for now
    const auto& soundEntry = soundArray[soundIndex];

    // Get factory
    if (!soundEntry.HasMember(kClassTag) || !soundEntry[kClassTag].IsString()) {
      MCLOG(Error, "No class tag for sound");
      continue;
    }
    std::string className(soundEntry[kClassTag].GetString());

    const auto& soundInfoMap = SoundFactory::GetInfoMap();
    const auto& soundInfo = soundInfoMap.find(className);
    if (soundInfo == soundInfoMap.end()) {
      MCLOG(Error, "Invalid class tag for sound");
      continue;
    }

    try {
      AddSound(soundInfo->second.serialize({ soundEntry }));
    }
    catch (...) {
      continue;
    }
  }

  return true;
}

bool Patch::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.Key(kPatchTag);
  w.StartObject();

  // Processes
  w.Key(kProcessesTag);
  w.StartArray();
  {
    for (auto& process : processes) {
      w.StartObject();
      {
        w.Key(kClassTag);
        w.String(process->GetProcessClassName().c_str());

        process->SerializeWrite(serializer);

        w.EndObject();
      }
    }
    w.EndArray();
  }


  // Sounds
  w.Key(kSoundsTag);
  w.StartArray();
  {
    for (auto& sound : sounds) {
      w.StartObject();
      {
        w.Key(kClassTag);
        w.String(sound->GetSoundClassName().c_str());

        sound->SerializeWrite(serializer);

        w.EndObject();
      }
    }
    w.EndArray();
  }

  w.EndObject();

  return true;
}

void Patch::AddSound(Sound* sound) {
  assert(sound);

  sounds.push_back(sound);

  // Get duration
  soundDuration = 0.0f;
  for (const auto& sound : sounds) {
    if (sound->GetDuration() > soundDuration) {
      soundDuration = sound->GetDuration();
    }
  }
}

void Patch::AddProcess(Process* process) {
  assert(process);

  processes.push_back(process);
}

void Patch::RenderDialog() {
  static std::string processName;
  static std::string soundName;

  for (int i = 0; i < kSubDialogPaddingSpacing; ++i) {
    ImGui::Spacing();
  }

  ImGui::Text("Processes");
  ImGui::SameLine(ImGui::GetWindowWidth() - 30);
  ImGui::PushID(&processName);
  if (ImGui::Button("+")) {
    ImGui::PopID();
    processName = "ProcessDecay";
    ImGui::OpenPopup("Add Process");
  }
  else {
    ImGui::PopID();
  }

  if (ImGui::BeginPopup("Add Process")) {
    auto const& processInfoMap = ProcessFactory::GetInfoMap();
    if (ImGui::BeginCombo("Process", processName.c_str())) {
      for (auto const& processInfo : processInfoMap) {
        ImGui::PushID(reinterpret_cast<const void*>(&processInfo.second));
        if (ImGui::Selectable(processInfo.first.c_str(), processInfo.first == processName)) {
          processName = processInfo.first;
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }

    ImGui::Spacing();

    if (ImGui::Button("OK")) {
      AddProcess(ProcessFactory::GetInfoMap().find(processName)->second.spawn());

      processName.clear();

      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

//  ImGui::DrawRect(ImVec2(ImGui::GetWindowSize().x -
//    kScrollBarWidth, 200.0f), ImGui::ColorConvertFloat4ToU32(ImColor(0.6f, 0.6f, 0.6f, 1.0f)));

  ImGui::BeginChild("#ProcessScrollingRegion",
    ImVec2(ImGui::GetWindowSize().x - kScrollBarWidth, 200.0f),
    true,
    ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
  {
    auto processIt = processes.begin();
    while (processIt != processes.end()) {
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::Text((*processIt)->GetProcessClassName().c_str());
      ImGui::SameLine(ImGui::GetWindowSize().x - kScrollBarWidth - 22.0f);
      if (processes.size() == 1) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      }
      std::string removeTag = std::string("RemoveProcess") +
        std::to_string(reinterpret_cast<uint32>(*processIt));
      ImGui::PushID(removeTag.c_str());
      bool remove = ImGui::Button("x");
      ImGui::PopID();
      if (processes.size() == 1) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
      }
      if (remove) {
        delete *processIt;
        processIt = processes.erase(processIt);
      }
      else {
        (*processIt)->RenderDialog();
        ++processIt;
      }
      ImGui::Spacing();
    }
    ImGui::Separator();
    ImGui::EndChild();
  }

  for (int i = 0; i < kSubDialogPaddingSpacing; ++i) {
    ImGui::Spacing();
  }

  ImGui::Text("Sounds");
  ImGui::SameLine(ImGui::GetWindowWidth() - 30);
  ImGui::PushID(&soundName);
  if (ImGui::Button("+")) {
    ImGui::PopID();
    soundName = "WavSound";
    ImGui::OpenPopup("Add Sound");
  }
  else {
    ImGui::PopID();
  }

  if (ImGui::BeginPopup("Add Sound")) {
    auto const& soundInfoMap = SoundFactory::GetInfoMap();
    if (ImGui::BeginCombo("Sound", soundName.c_str())) {
      for (auto const& soundInfo : soundInfoMap) {
        ImGui::PushID(reinterpret_cast<const void*>(&soundInfo.second));
        if (ImGui::Selectable(soundInfo.first.c_str(), soundInfo.first == soundName)) {
          soundName = soundInfo.first;
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }

    ImGui::Spacing();

    if (ImGui::Button("OK")) {
      AddSound(SoundFactory::GetInfoMap().find(soundName)->second.spawn());

      soundName.clear();

      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  ImGui::BeginChild("#SoundsScrollingRegion",
    ImVec2(ImGui::GetWindowSize().x - kScrollBarWidth, 200.0f),
    true,
    ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
  {
    auto soundIt = sounds.begin();
    while (soundIt != sounds.end()) {
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::Text((*soundIt)->GetSoundClassName().c_str());
      ImGui::SameLine(ImGui::GetWindowSize().x - kScrollBarWidth - 22.0f);
      if (sounds.size() == 1) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
      }
      std::string removeTag = std::string("RemoveSound") +
        std::to_string(reinterpret_cast<uint32>(*soundIt));
      ImGui::PushID(removeTag.c_str());
      bool remove = ImGui::Button("x");
      ImGui::PopID();
      if (sounds.size() == 1) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
      }
      if (remove) {
        delete *soundIt;
        soundIt = sounds.erase(soundIt);
      }
      else {
        (*soundIt)->RenderDialog();
        ++soundIt;
      }
      ImGui::Spacing();
    }
    ImGui::Separator();
    ImGui::EndChild();
  }

  for (int i = 0; i < kSubDialogPaddingSpacing; ++i) {
    ImGui::Spacing();
  }
}

