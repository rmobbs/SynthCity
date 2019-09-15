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
  if (patch.HasMember(kProcessesTag)) {
    if (!patch[kProcessesTag].IsArray()) {
      MCLOG(Error, "Invalid process array in patch");
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
  }

  // Sounds
  if (patch.HasMember(kSoundsTag)) {
    if (!patch[kSoundsTag].IsArray()) {
      MCLOG(Error, "Invalid sound array in patch");
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
  }

  return true;
}

bool Patch::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.Key(kPatchTag);
  w.StartObject();

  // Processes
  if (processes.size()) {
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
  }

  // Sounds
  if (sounds.size()) {
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
  }

  w.EndObject();

  return true;
}

void Patch::UpdateDuration() {
  // Get duration
  soundDuration = 0.0f;
  for (const auto& sound : sounds) {
    if (sound->GetDuration() > soundDuration) {
      soundDuration = sound->GetDuration();
    }
  }
}

void Patch::AddSound(Sound* sound) {
  assert(sound);

  sounds.push_back(sound);
  UpdateDuration();
}

void Patch::RemoveSound(Sound* sound) {
  assert(sound);
  auto soundEntry = std::find(sounds.begin(), sounds.end(), sound);
  if (soundEntry != sounds.end()) {
    delete *soundEntry;
    sounds.erase(soundEntry);
  }
}

void Patch::AddProcess(Process* process) {
  assert(process);

  processes.push_back(process);
}

void Patch::RemoveProcess(Process* process) {
  assert(process);
  auto processEntry = std::find(processes.begin(), processes.end(), process);
  if (processEntry != processes.end()) {
    delete* processEntry;
    processes.erase(processEntry);
  }
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
  bool disableAddProcess = processes.size() >= kMaxProcesses;
  if (disableAddProcess) {
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
  }
  if (ImGui::Button("+")) {
    ImGui::PopID();
    processName = "ProcessDecay";
    ImGui::OpenPopup("Add Process");
  }
  else {
    ImGui::PopID();
  }
  if (disableAddProcess) {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
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

  static constexpr float kProcessRegionPercentage = 0.35f;
  static constexpr float kSoundRegionPercentage = 0.35f;

  ImGui::BeginChild("#ProcessScrollingRegion",
    ImVec2(ImGui::GetWindowSize().x - Globals::kScrollBarWidth, ImGui::GetWindowSize().y * kProcessRegionPercentage),
    true,
    ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
  {
    Process* remove = nullptr;
    for (auto& process : processes) {
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::Text(process->GetProcessClassName().c_str());
      ImGui::SameLine(ImGui::GetWindowSize().x - Globals::kScrollBarWidth - 22.0f);
      std::string removeTag = std::string("RemoveProcess") +
        std::to_string(reinterpret_cast<uint32>(process));
      ImGui::PushID(removeTag.c_str());
      if (ImGui::Button("x")) {
        remove = process;
      }
      ImGui::PopID();
      process->RenderDialog();
      ImGui::Spacing();
    }
    if (processes.size()) {
      ImGui::Separator();
    }
    ImGui::EndChild();

    if (remove) {
      RemoveProcess(remove);
    }
  }

  for (int i = 0; i < kSubDialogPaddingSpacing; ++i) {
    ImGui::Spacing();
  }

  ImGui::Text("Sounds");
  ImGui::SameLine(ImGui::GetWindowWidth() - 30);
  ImGui::PushID(&soundName);
  bool disableAddSound = sounds.size() >= kMaxSounds;
  if (disableAddSound) {
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
  }
  if (ImGui::Button("+")) {
    ImGui::PopID();
    soundName = "WavSound";
    ImGui::OpenPopup("Add Sound");
  }
  else {
    ImGui::PopID();
  }
  if (disableAddSound) {
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
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
    ImVec2(ImGui::GetWindowSize().x - Globals::kScrollBarWidth, ImGui::GetWindowSize().y * kSoundRegionPercentage),
    true,
    ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
  {
    Sound* remove = nullptr;
    for (auto& sound : sounds) {
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::Text(sound->GetSoundClassName().c_str());
      ImGui::SameLine(ImGui::GetWindowSize().x - Globals::kScrollBarWidth - 22.0f);
      std::string removeTag = std::string("RemoveSound") +
        std::to_string(reinterpret_cast<uint32>(sound));
      ImGui::PushID(removeTag.c_str());
      if (ImGui::Button("x")) {
        remove = sound;
      }
      ImGui::PopID();
      sound->RenderDialog();
      ImGui::Spacing();
    }
    if (sounds.size()) {
      ImGui::Separator();
    }
    ImGui::EndChild();

    if (remove) {
      RemoveSound(remove);
    }
  }

  for (int i = 0; i < kSubDialogPaddingSpacing; ++i) {
    ImGui::Spacing();
  }
}

