#include "DialogInstrument.h"
#include "SerializeImpl.h"
#include "Process.h"
#include "Sound.h"
#include "Sequencer.h"
#include "Instrument.h"
#include "Song.h"
#include "OddsAndEnds.h"
#include "ImGuiExtensions.h"
#include "imgui.h"
#include "imgui_internal.h"

static constexpr float kMinDialogWidth(400.0f);
static constexpr float kMinDialogHeight(400.0f);
static constexpr float kColorKeysRegionPercentage = 0.7f;
static constexpr const char* kDefaultColorKeyName("ColorKey");
static constexpr float kColorPreviewWidth(72.0f);

static const std::pair<const char*, const char*> kPaletteLabelsAndTooltips[Instrument::kColorPaletteSize] = {
  { "B",  "Background normal color" },
  { "BH", "Background highlighted color" },
  { "T",  "Text normal color" },
};

bool AlreadyUsed(const std::vector<std::pair<std::string, std::array<uint32, Instrument::kColorPaletteSize>>>& keys, std::string newName) {
  for (const auto& key : keys) {
    if (key.first == newName) {
      return true;
    }
  }
  return false;
}

DialogInstrument::DialogInstrument(std::string title, Instrument* instrument)
  : title(title)
  , instrument(instrument) {
  for (const auto& palette : instrument->GetTrackPalette()) {
    trackPalette.push_back({ palette.first, palette.second });
  }
}

DialogInstrument::~DialogInstrument() {

}

void DialogInstrument::Open() {
  wasPlaying = Sequencer::Get().IsPlaying();
  Sequencer::Get().PauseKill();
  ImGui::OpenPopup(title.c_str());
}

std::string DialogInstrument::GetUniqueColorKeyName(std::string nameBase) {
  // Pick an available name
  std::string newName = nameBase;

  // Has to end sometime
  for (int nameSuffix = 1; nameSuffix < 1000; ++nameSuffix) {
    auto itemIter = trackPalette.begin();
    while (itemIter != trackPalette.end()) {
      if (itemIter->first == newName) {
        break;
      }
      ++itemIter;
    }

    if (itemIter == trackPalette.end()) {
      break;
    }

    newName = nameBase + std::string(" (") + std::to_string(nameSuffix) + std::string(")");
  }

  return newName;
}

bool DialogInstrument::Render() {
  // At this point in time, the data edited in this dialog are all non-runtime-critical so we
  // can just directly update the instrument via pointer
  ImGui::SetNextWindowSizeConstraints(ImVec2(kMinDialogWidth, kMinDialogHeight), ImVec2(1.0e9f, 1.0e9f));
  bool isOpen = true;
  if (ImGui::BeginPopupModal(title.c_str(), &isOpen)) {
    // Instrument name
    char nameBuf[1024];
    strcpy_s(nameBuf, sizeof(nameBuf), instrument->GetName().c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
      instrument->SetName(std::string(nameBuf));
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    auto& imGuiStyle = ImGui::GetStyle();

    auto defaultItemSpacing = imGuiStyle.ItemSpacing;

    // Palette
    ImGui::Text("Palette");
    ImGui::SameLine(ImGui::GetWindowWidth() - 30);
    if (ImGui::Button("+")) {
      trackPalette.push_back({ GetUniqueColorKeyName(kDefaultColorKeyName), { 0 } });
    }

    auto scrollRegionWidth = ImGui::GetWindowSize().x - Globals::kScrollBarWidth;
    ImGui::BeginChild("#ProcessScrollingRegion",
      ImVec2(scrollRegionWidth, ImGui::GetWindowSize().y * kColorKeysRegionPercentage),
      true,
      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
      int32 removeKeyIndex = -1;
      
      UniqueIdBuilder<64> uniqueIdBuilderName("din:");
      UniqueIdBuilder<64> uniqueIdBuilderClose("dic:");
      UniqueIdBuilder<64> uniqueIdBuilderColor("dib:");

      scrollRegionWidth -= Globals::kScrollBarWidth;

      auto colorColumnWidth = Globals::kKeyboardKeyHeight + defaultItemSpacing.x * 2.0f;

      ImGui::Columns(4);
      ImGui::SetColumnWidth(0, scrollRegionWidth - colorColumnWidth * 4.0f - Globals::kKeyboardKeyHeight);
      ImGui::Text("Name");
      ImGui::NextColumn();
      for (uint32 paletteIndex = 0; paletteIndex < Instrument::kColorPaletteSize; ++paletteIndex) {
        ImGui::SetColumnWidth(paletteIndex + 1, colorColumnWidth);
        ImGui::Text(kPaletteLabelsAndTooltips[paletteIndex].first);
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(kPaletteLabelsAndTooltips[paletteIndex].second);
        }
        ImGui::NextColumn();
      }

      for (size_t keyIndex = 0; keyIndex < trackPalette.size(); ++keyIndex) {
        uniqueIdBuilderName.PushUnsigned(keyIndex);
        uniqueIdBuilderClose.PushUnsigned(keyIndex);
        uniqueIdBuilderColor.PushUnsigned(keyIndex);

        // X
        imGuiStyle.ItemSpacing.x = 1.0f;
        ImGui::PushID(uniqueIdBuilderClose());
        if (ImGui::Button("x", ImVec2(Globals::kKeyboardKeyHeight, Globals::kKeyboardKeyHeight))) {
          removeKeyIndex = keyIndex;
        }
        ImGui::PopID();

        ImGui::SameLine();
        imGuiStyle.ItemSpacing.x = defaultItemSpacing.x;

        // Name
        char nameBuf[1024];
        strcpy_s(nameBuf, sizeof(nameBuf), trackPalette[keyIndex].first.c_str());
        ImGui::PushID(uniqueIdBuilderName());
        if (ImGui::InputTextEx("", "<Name>", nameBuf, sizeof(nameBuf), ImVec2(ImGui::GetColumnWidth(-1) -
          Globals::kKeyboardKeyHeight - defaultItemSpacing.x, Globals::kKeyboardKeyHeight), 0)) {
          std::string newKeyName(nameBuf);
          if (!newKeyName.empty() && !AlreadyUsed(trackPalette, newKeyName)) {
            trackPalette[keyIndex].first = newKeyName;
          }
        }
        ImGui::PopID();
        ImGui::NextColumn();
        
        for (size_t colorIndex = 0; colorIndex < trackPalette[keyIndex].second.size(); ++colorIndex) {
          auto vecColor = ImGui::ColorConvertU32ToFloat4(trackPalette[keyIndex].second[colorIndex]);

          float colors[4] = { vecColor.x, vecColor.y, vecColor.z, vecColor.w };
          uniqueIdBuilderColor.PushUnsigned(colorIndex);
          ImGui::PushID(uniqueIdBuilderColor());
          if (ImGui::ColorEdit4("", colors, ImGuiColorEditFlags_NoInputs)) {
            vecColor.x = colors[0];
            vecColor.y = colors[1];
            vecColor.z = colors[2];
            vecColor.w = colors[3];

            trackPalette[keyIndex].second[colorIndex] = ImGui::ColorConvertFloat4ToU32(vecColor);
          }
          ImGui::PopID();
          uniqueIdBuilderColor.Pop();
          ImGui::NextColumn();
        }

        uniqueIdBuilderColor.Pop();
        uniqueIdBuilderClose.Pop();
        uniqueIdBuilderName.Pop();
      }

      ImGui::EndChild();
      ImGui::Spacing();

      // Removed key
      if (removeKeyIndex != -1) {
        trackPalette.erase(trackPalette.begin() + removeKeyIndex);
        removeKeyIndex = -1;
      }
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

void DialogInstrument::Close() {
  if (wasPlaying) {
    Sequencer::Get().Play();
    wasPlaying = false;
  }

  if (exitedOk) {
    std::map<std::string, std::array<uint32, Instrument::kColorPaletteSize>> newColorKeys;
    for (auto& palette : trackPalette) {
      newColorKeys.emplace(palette.first, palette.second);
    }
    instrument->SetColorKeys(std::move(newColorKeys));
  }
}

