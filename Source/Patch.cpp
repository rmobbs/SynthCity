#include "Patch.h"
#include "Logging.h"
#include "SerializeImpl.h"
#include "ProcessFactory.h"
#include "SoundFactory.h"
#include "ProcessDecay.h"
#include "Sound.h"
#include "imgui.h"

#include <stdexcept>

static constexpr const char* kPatchTag("patch");
static constexpr const char* kProcessesTag("processes");
static constexpr const char* kSoundsTag("sounds");
static constexpr uint32 kSubDialogPaddingSpacing = 5;

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
  if (!patch.HasMember(kProcessesTag) || !patch[kProcessesTag].IsArray()) {
    MCLOG(Error, "Missing or invalid process array in patch");
    return false;
  }
  const auto& processesArray = patch[kProcessesTag];
  if (processesArray.Size()) {
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
        processes.push_back(processInfo->second.factory({ processEntry }));
      }
      catch (...) {
        continue;
      }
    }
  }
  else {
    // If no process is attached, create a decay 0 process; this will always
    // return true and allow the sound to naturally expire (if it does)
    processes.push_back(new ProcessDecay);
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
      sounds.push_back(soundInfo->second.factory({ soundEntry }));
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

void Patch::RenderDialog() {
  static std::string processName;
  static std::string soundName;

  for (int i = 0; i < kSubDialogPaddingSpacing; ++i) {
    ImGui::Spacing();
  }

  ImGui::Text("Processes");
  ImGui::SameLine(ImGui::GetWindowWidth() - 30);
  if (ImGui::Button("+")) {
    processName = "ProcessDecay";
    ImGui::OpenPopup("Add Process");
  }

  if (ImGui::BeginPopup("Add Process")) {
    auto const& processInfoMap = ProcessFactory::GetInfoMap();
    if (ImGui::BeginCombo("Process", processName.c_str())) {
      for (auto const& processInfo : processInfoMap) {
        ImGui::PushID(reinterpret_cast<const void*>(&processInfo.second));
        if (ImGui::Selectable(processInfo.first.c_str(), processInfo.first == processName)) {
          processName = processInfo.first;

          // TODO: Spawn/add process
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }

    ImGui::Spacing();

    ImGui::Button("OK");
    ImGui::SameLine();
    ImGui::Button("Cancel");

    ImGui::EndPopup();
  }

  for (auto& process : processes) {
    ImGui::Separator();
    ImGui::Text(process->GetProcessClassName().c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 30);
    ImGui::Button("x");
    process->RenderDialog();
  }
  ImGui::Separator();

  for (int i = 0; i < kSubDialogPaddingSpacing; ++i) {
    ImGui::Spacing();
  }

  ImGui::Text("Sounds");
  ImGui::SameLine(ImGui::GetWindowWidth() - 30);
  ImGui::Button("+");

  for (auto& sound : sounds) {
    ImGui::Separator();
    ImGui::Text(sound->GetSoundClassName().c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 30);
    ImGui::Button("x");
    sound->RenderDialog();
  }
  ImGui::Separator();

  for (int i = 0; i < kSubDialogPaddingSpacing; ++i) {
    ImGui::Spacing();
  }
}

