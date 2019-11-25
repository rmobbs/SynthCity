#include "SongTab.h"
#include "Sequencer.h"
#include "Song.h"
#include "InputState.h"
#include "OddsAndEnds.h"
#include "DialogOptions.h"
#include "InstrumentBank.h"
#include "ComposerView.h"

#include "SDL.h"
#include "soil.h"
#include "GL/glew.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuiExtensions.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
#include <commdlg.h>
#include <atlbase.h>
#include <algorithm>

static constexpr uint32 kMinMeasuresToAdd = 1;
static constexpr uint32 kMaxMeasuresToAdd = 256;
static const ImVec4 kDragBoxOutlineColor(0.0f, 0.0f, 0.0f, 1.0f);
static const ImVec4 kDragBoxFillColor(0.9f, 0.9f, 0.9f, 0.5f);
static const ImVec4 kDragSelectColor(1.0f, 1.0f, 0.0f, 1.0f);
static const ImVec4 kMeasureDemarcationLineColor(1.0f, 1.0f, 1.0f, 1.0f);
static const ImVec4 kBeatDemarcationLineColor(0.6f, 0.6f, 0.6f, 1.0f);
static const ImVec4 kPlayLineColor(1.0f, 1.0f, 0.0f, 1.0f);
static const ImVec4 kDefaultNoteColor(1.0f, 1.0f, 1.0f, 0.5f);
static const ImVec4 kFretColors[] = {
  ImVec4(1.0f, 0.0f, 0.0f, 0.5f),
  ImVec4(0.0f, 1.0f, 0.0f, 0.5f),
  ImVec4(0.0f, 0.0f, 1.0f, 0.5f),
  ImVec4(1.0f, 1.0f, 0.0f, 0.5f),
};


template<typename T> inline void map2remove(std::map<const InstrumentInstance*, std::map<uint32, T>>& outerMap, const InstrumentInstance* outerMapKey, const uint32 innerMapKey) {
  auto innerMap = outerMap.find(outerMapKey);
  if (innerMap != outerMap.end()) {
    auto leafItem = innerMap->second.find(innerMapKey);
    if (leafItem != innerMap->second.end()) {
      innerMap->second.erase(leafItem);
    }
  }
}

template<typename T> inline void map2remove(std::map<InstrumentInstance*, std::map<uint32, T>>& outerMap, InstrumentInstance* outerMapKey, uint32 innerMapKey) {
  auto innerMap = outerMap.find(outerMapKey);
  if (innerMap != outerMap.end()) {
    auto leafItem = innerMap->second.find(innerMapKey);
    if (leafItem != innerMap->second.end()) {
      innerMap->second.erase(leafItem);
    }
  }
}

SongTab::SongTab(ComposerView* composerView)
: View("Song", nullptr)
, composerView(composerView) {
  InitResources();
}

SongTab::~SongTab() {

}

void SongTab::InitResources() {
  // Backup GL state
  GLint lastTexture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);

  // Load UI textures
  int width, height;
  uint8* iconData = nullptr;
  glGenTextures(1, &pauseButtonIconTexture);
  glBindTexture(GL_TEXTURE_2D, pauseButtonIconTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  iconData = SOIL_load_image("Assets\\pause_icon.png", &width, &height, 0, SOIL_LOAD_RGBA);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconData);
  SOIL_free_image_data(iconData);

  // Set font texture ID
  glGenTextures(1, &stopButtonIconTexture);
  glBindTexture(GL_TEXTURE_2D, stopButtonIconTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  iconData = SOIL_load_image("Assets\\stop_icon.png", &width, &height, 0, SOIL_LOAD_RGBA);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconData);
  SOIL_free_image_data(iconData);

  // Restore original state
  glBindTexture(GL_TEXTURE_2D, lastTexture);
}

void SongTab::Show() {

}

void SongTab::Hide() {

}

std::string SongTab::GetUniqueInstrumentInstanceName(std::string instrumentInstanceNameBase) {
  // Pick an available name
  std::string instrumentInstanceName = instrumentInstanceNameBase;

  // Has to end sometime
  const auto& instrumentInstanceDataMap = Sequencer::Get().GetSong()->GetInstrumentInstances();
  for (int nameSuffix = 1; nameSuffix < 1000; ++nameSuffix) {
    auto instrumentInstanceData = instrumentInstanceDataMap.begin();
    while (instrumentInstanceData != instrumentInstanceDataMap.end()) {
      if ((*instrumentInstanceData)->GetName() == instrumentInstanceName) {
        break;
      }
      ++instrumentInstanceData;
    }

    if (instrumentInstanceData == instrumentInstanceDataMap.end()) {
      break;
    }

    instrumentInstanceName = std::string(instrumentInstanceNameBase) +
      std::string(" [") + std::to_string(nameSuffix) + std::string("]");
  }

  return instrumentInstanceName;
}

void SongTab::NewSong() {
  auto song = new Song(Song::kDefaultName, Globals::kDefaultTempo,
    Song::kDefaultNumMeasures, Song::kDefaultBeatsPerMeasure, Globals::kDefaultMinNote);

  selectedNotesByInstrumentInstance.clear();
  selectingNotesByInstrumentInstance.clear();
  Sequencer::Get().SetSong(song);
}

void SongTab::LoadSong() {
  WCHAR szFile[FILENAME_MAX] = { 0 };
  OPENFILENAME ofn = { 0 };

  USES_CONVERSION;
  std::string windowTitle("Load Song");
  ofn.lpstrTitle = A2W(windowTitle.c_str());
  ofn.hwndOwner = reinterpret_cast<HWND>(Globals::mainWindowHandle);
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
  // TODO: Fix MIDI loading
  // https://trello.com/c/vQCRzrcm
  ofn.lpstrFilter = _TEXT("JSON\0*.json\0");//MIDI\0 * .midi; *.mid\0");
  ofn.nFilterIndex = 0;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileName(&ofn)) {
    auto song = Song::LoadSong(W2A(szFile));

    if (song != nullptr) {
      selectedNotesByInstrumentInstance.clear();
      selectingNotesByInstrumentInstance.clear();
      Sequencer::Get().SetSong(song);
    }
  }
}

void SongTab::SaveSong() {
  WCHAR szFile[FILENAME_MAX] = { 0 };
  OPENFILENAME ofn = { 0 };

  USES_CONVERSION;
  std::string windowTitle("Save Song");
  ofn.lpstrTitle = A2W(windowTitle.c_str());
  ofn.hwndOwner = reinterpret_cast<HWND>(Globals::mainWindowHandle);
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
  ofn.lpstrFilter = _TEXT("JSON\0*.json\0");
  ofn.nFilterIndex = 0;
  ofn.Flags = OFN_OVERWRITEPROMPT;

  if (GetSaveFileName(&ofn)) {
    Sequencer::Get().GetSong()->Save(W2A(szFile));
  }
}

void SongTab::SelectedGroupAction(std::function<void(InstrumentInstance*, uint32, uint32)> action) {
  for (const auto& instrumentInstance : selectedNotesByInstrumentInstance) {
    for (const auto& trackId : instrumentInstance.second) {
      for (const auto& note : trackId.second) {
        action(instrumentInstance.first, trackId.first, note);
      }
    }
  }
}

void SongTab::HandleInput() {
  auto& inputState = InputState::Get();

  if (inputState.pressed[SDLK_DELETE]) {
    SelectedGroupAction([=](InstrumentInstance* instrumentInstance, uint32 trackId, uint32 beatIndex) {
      instrumentInstance->RemoveNote(trackId, beatIndex);
      });

    selectedNotesByInstrumentInstance.clear();
  }

  if (inputState.pressed[SDLK_ESCAPE]) {
    selectedNotesByInstrumentInstance.clear();
  }

  if (inputState.pressed[SDLK_SPACE]) {
    if (Sequencer::Get().IsPlaying()) {
      Sequencer::Get().Stop();
    }
    else {
      Sequencer::Get().Play();
    }
  }

  int32 newGameIndex = -2;

  if (inputState.pressed[SDLK_1]) {
    newGameIndex = 0;
  }
  else if (inputState.pressed[SDLK_2]) {
    newGameIndex = 1;
  }
  else if (inputState.pressed[SDLK_3]) {
    newGameIndex = 2;
  }
  else if (inputState.pressed[SDLK_4]) {
    newGameIndex = 3;
  }
  else if (inputState.pressed[SDLK_0]) {
    newGameIndex = -1;
  }

  if (newGameIndex != -2) {
    SelectedGroupAction([=](InstrumentInstance* instrumentInstance, uint32 trackId, uint32 beatIndex) {
      instrumentInstance->SetNoteGameIndex(trackId, beatIndex, newGameIndex);
      });
  }

  hoveredNoteInstance = { };
}

void SongTab::DoLockedActions() {
  auto& sequencer = Sequencer::Get();

  // Newly triggered notes are written to entry 1 in the audio callback
  playingTrackFlashTimes[0].merge(playingTrackFlashTimes[1]);

  // Subdivision changed
  if (pendingSubdivision != kInvalidUint32) {
    sequencer.SetSubdivision(pendingSubdivision);
  }

  auto song = sequencer.GetSong();
  if (song != nullptr) {
    // Track instance mute state changed
    if (pendingMuteTrackInstance.instance != nullptr) {
      pendingMuteTrackInstance.instance->SetTrackMute(pendingMuteTrackInstance.trackId, pendingMuteTrackInstance.data);
    }

    // Track instance solo state changed
    auto& soloTrackIndex = composerView->GetSoloTrackInstance();
    if (pendingSoloTrackInstance.data) {
      composerView->SetSoloTrackInstance(pendingSoloTrackInstance.instance, pendingSoloTrackInstance.trackId);
    }

    // Add voice
    if (pendingPlayTrackInstance.instance != nullptr) {
      auto track = pendingPlayTrackInstance.instance->instrument->GetTrackById(pendingPlayTrackInstance.trackId);
      if (track != nullptr) {
        Sequencer::Get().PlayPatch(track->GetPatch(), track->GetVolume());
      }
    }

    // Move instrument instance up/down
    if (pendingMoveInstrumentInstance.instance != nullptr) {
      song->MoveInstrumentInstance(pendingMoveInstrumentInstance.instance, pendingMoveInstrumentInstance.data);
    }

    // Create instrument instance
    if (pendingCreateInstrumentInstance != nullptr) {
      auto instrumentInstance = pendingCreateInstrumentInstance->instrument->Instance();
      instrumentInstance->SetName(GetUniqueInstrumentInstanceName(pendingCreateInstrumentInstance->instrument->GetName()));
      song->AddInstrumentInstance(instrumentInstance);
    }

    // Remove instrument instance
    if (pendingRemoveInstrumentInstance != nullptr) {
      // Might be nice to let the other voices continue, but a lot of things would be nice
      bool wasPlaying = sequencer.IsPlaying();
      if (wasPlaying) {
        sequencer.PauseKill();
      }

      song->RemoveInstrumentInstance(pendingRemoveInstrumentInstance);

      if (wasPlaying) {
        sequencer.Play();
      }
    }

    // Toggle a note on
    if (pendingToggleNoteInstance.instance != nullptr) {
      pendingToggleNoteInstance.instance->
        AddNote(pendingToggleNoteInstance.trackId, pendingToggleNoteInstance.data, -1);
    }

    // Measures added
    if (pendingAddMeasures != kInvalidUint32) {
      song->AddMeasures(pendingAddMeasures);
    }

    // Beats per minute changed
    if (pendingTempo != kInvalidUint32) {
      song->SetTempo(pendingTempo);
      sequencer.UpdateInterval();
    }

    if (pendingSaveSong) {
      song->Save();
    }

    if (pendingSaveAsSong) {
      SaveSong();
    }
  }

  if (pendingAddInstrument) {
    selectedNotesByInstrumentInstance.clear();
    auto instrument = InstrumentBank::Get().LoadInstrumentName({ }, false);
    if (instrument != nullptr) {
      Sequencer::Get().StopKill();
      auto instrumentInstance = instrument->Instance();
      instrumentInstance->SetName(GetUniqueInstrumentInstanceName(instrument->GetName()));
      song->AddInstrumentInstance(instrumentInstance);
    }
  }

  if (pendingNewSong) {
    selectedNotesByInstrumentInstance.clear();
    NewSong();
  }

  if (pendingLoadSong) {
    selectedNotesByInstrumentInstance.clear();
    LoadSong();
  }

  // Reset all pendings
  pendingSubdivision = kInvalidUint32;
  pendingTempo = kInvalidUint32;
  pendingMuteTrackInstance = { };
  pendingPlayTrackInstance = { };
  pendingAddMeasures = kInvalidUint32;
  pendingAddInstrument = false;
  pendingNewSong = false;
  pendingLoadSong = false;
  pendingSaveSong = false;
  pendingSaveAsSong = false;
  pendingToggleNoteInstance = { };
  pendingMoveInstrumentInstance = { };
  pendingSoloTrackInstance = { };
  pendingCreateInstrumentInstance = nullptr;
  pendingRemoveInstrumentInstance = nullptr;
}

void SongTab::ConditionalEnableBegin(bool condition) {
  localGuiDisabled = !condition;
  if (localGuiDisabled) {
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
  }
}

void SongTab::ConditionalEnableEnd() {
  if (localGuiDisabled) {
    ImGui::PopStyleVar();
    ImGui::PopItemFlag();
  }
  localGuiDisabled = false;
}

void SongTab::DoMainMenuBar() {
  auto& sequencer = Sequencer::Get();

  auto song = sequencer.GetSong();

  if (ImGui::BeginMenu("Song")) {
    if (ImGui::MenuItem("New")) {
      pendingNewSong = true;
    }
    if (ImGui::MenuItem("Load")) {
      pendingLoadSong = true;
    }

    ConditionalEnableBegin(song != nullptr && !song->GetFileName().empty());
    if (ImGui::MenuItem("Save")) {
      pendingSaveSong = true;
    }
    ConditionalEnableEnd();

    ConditionalEnableBegin(song != nullptr);
    if (ImGui::MenuItem("Save As")) {
      pendingSaveAsSong = true;
    }

    if (ImGui::MenuItem("Add Instrument")) {
      pendingAddInstrument = true;
    }

    ConditionalEnableEnd();

    ImGui::EndMenu();
  }
}

void SongTab::Render(ImVec2 canvasSize) {
  auto& sequencer = Sequencer::Get();
  auto song = sequencer.GetSong();
  auto imGuiFont = ImGui::GetFont();
  auto& imGuiStyle = ImGui::GetStyle();
  auto defaultItemSpacing = imGuiStyle.ItemSpacing;
  auto defaultFramePadding = imGuiStyle.FramePadding;

  static constexpr float kTopToolbarHeight = 26.0f;
  static constexpr float kBottomToolbarHeight = 50.0f;

  if (song != nullptr) {
    // Top bar
    {
      std::string songName("Song");
      char newSongName[256] = { 0 };
      strcpy(newSongName, song->GetName().c_str());
      if (ImGui::InputText(songName.c_str(), newSongName, _countof(newSongName) - 1)) {
        song->SetName(std::string(newSongName));
      }
      ImGui::SameLine();

      ImGui::PushItemWidth(100);

      int currentBpm = song->GetTempo();
      if (ImGui::InputInt("BPM", &currentBpm)) {
        pendingTempo = std::max(static_cast<uint32>(currentBpm), Globals::kMinTempo);
      }

      ImGui::SameLine();

      imGuiStyle.ItemSpacing.x = 4;

      int measuresToAdd = addMeasureCount;
      if (ImGui::InputInt("", &measuresToAdd)) {
        addMeasureCount = std::min(std::max(kMinMeasuresToAdd,
          static_cast<uint32>(measuresToAdd)), kMaxMeasuresToAdd);
      }

      ImGui::SameLine();

      if (ImGui::Button("Add measure(s)")) {
        pendingAddMeasures = addMeasureCount;
      }

      imGuiStyle.ItemSpacing.x = defaultItemSpacing.x;

      ImGui::Separator();
    }

    ImVec4 defaultColors[ImGuiCol_COUNT];
    memcpy(defaultColors, imGuiStyle.Colors, sizeof(defaultColors));

    ImVec2 trackAndNotesCanvasSize(canvasSize.x,
      canvasSize.y - kTopToolbarHeight - kBottomToolbarHeight);

    // Instrument/tracks child (left side, vertical scroll only)
    ImGui::BeginChild("##SongTabTrackNotesRegion",
      trackAndNotesCanvasSize,
      false,
      ImGuiWindowFlags_AlwaysAutoResize);
    {
      auto trackLabelWidth = imGuiFont->CalcTextSizeA(imGuiFont->
        FontSize, FLT_MAX, 0.0f, Globals::kTrackNameFormat).x;
      auto trackTotalWidth = trackLabelWidth + Globals::kHamburgerMenuWidth + defaultItemSpacing.x;

      // Use the song's instrument list so we always render in the correct order
      const auto& instrumentInstances = song->GetInstrumentInstances();
      for (const auto& instrumentInstance : instrumentInstances) {
        const auto instrument = instrumentInstance->instrument;

        if (instrumentInstance != instrumentInstances.front()) {
          // Delinate the instruments
          ImGui::Separator();
        }
        else {
          // Leave vertical space for the measure labels in the song line area
          ImGui::NewLine();
        }

        imGuiStyle.ItemSpacing.x = 0.0f;

        // Instrument hamburger menu
        ImGui::PushID(instrumentInstance->uniqueGuiIdHamburgerMenu.c_str());
        if (ImGui::Button("=", ImVec2(Globals::kHamburgerMenuWidth, Globals::kKeyboardKeyHeight))) {
          ImGui::PopID();
          ImGui::OpenPopup(instrumentInstance->uniqueGuiIdPropertiesPop.c_str());
        }
        else {
          ImGui::PopID();
        }
        ImGui::SameLine();

        memcpy(imGuiStyle.Colors, defaultColors, sizeof(defaultColors));

        if (ImGui::BeginPopup(instrumentInstance->uniqueGuiIdPropertiesPop.c_str())) {
          bool closePopup = false;

          if (ImGui::MenuItem("Move Up")) {
            pendingMoveInstrumentInstance = { instrumentInstance, 0, -1 };
            closePopup = true;
          }

          if (ImGui::MenuItem("Move Down")) {
            pendingMoveInstrumentInstance = { instrumentInstance, 0, +1 };
            closePopup = true;
          }

          if (ImGui::MenuItem("Instance")) {
            pendingCreateInstrumentInstance = instrumentInstance;
            closePopup = true;
          }

          if (ImGui::MenuItem("Remove")) {
            pendingRemoveInstrumentInstance = instrumentInstance;
            closePopup = true;
          }

          if (closePopup) {
            ImGui::CloseCurrentPopup();
          }
          ImGui::EndPopup();
        }

        imGuiStyle.FramePadding.y = imGuiStyle.ItemSpacing.y * 0.5f;
        imGuiStyle.ItemSpacing.y = 0.0f;
        char newInstrumentName[256] = { 0 };
        strcpy(newInstrumentName, instrumentInstance->GetName().c_str());
        ImGui::PushID(instrumentInstance->uniqueGuiIdName.c_str());
        if (ImGui::InputTextEx("", nullptr, newInstrumentName,
          _countof(newInstrumentName) - 1, ImVec2(trackLabelWidth, Globals::kKeyboardKeyHeight), 0)) {
          instrumentInstance->SetName(std::string(newInstrumentName));
        }
        ImGui::PopID();

        imGuiStyle.ItemSpacing = defaultItemSpacing;
        imGuiStyle.FramePadding = defaultFramePadding;

        // Tracks
        auto& soloTrackInstance = composerView->GetSoloTrackInstance();
        for (const auto& trackInstance : instrumentInstance->trackInstances) {
          auto trackId = trackInstance.first;

          auto track = instrument->GetTrackById(trackId);
          assert(track != nullptr);

          composerView->SetTrackColors(instrumentInstance->instrument, track->GetColorScheme());

          bool isSoloTrack = std::make_pair(instrumentInstance, static_cast<int32>(trackId)) == soloTrackInstance;

          // Hamburger menu
          ImGui::PushID(trackInstance.second.uniqueGuiIdHamburgerMenu.c_str());
          if (ImGui::Button("=",
            ImVec2(Globals::kHamburgerMenuWidth, Globals::kKeyboardKeyHeight))) {
            ImGui::PopID();
            ImGui::OpenPopup(trackInstance.second.uniqueGuiIdPropertiesPop.c_str());
          }
          else {
            ImGui::PopID();
          }
          memcpy(imGuiStyle.Colors, defaultColors, sizeof(defaultColors));
          if (ImGui::BeginPopup(trackInstance.second.uniqueGuiIdPropertiesPop.c_str())) {
            bool closePopup = false;

            bool mute = trackInstance.second.mute;
            if (ImGui::Checkbox("Mute", &mute)) {
              pendingMuteTrackInstance = { instrumentInstance, trackId, mute };
              closePopup = true;
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Solo", &isSoloTrack)) {
              if (isSoloTrack) {
                pendingSoloTrackInstance = { instrumentInstance, trackId, true };
                pendingMuteTrackInstance = { instrumentInstance, trackId, false };
              }
              else {
                pendingSoloTrackInstance = { nullptr, kInvalidUint32, true };
              }
              closePopup = true;
            }

            if (closePopup) {
              ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
          }

          imGuiStyle.ItemSpacing.y = 0.0f;
          imGuiStyle.ItemSpacing.x = 0.0f;

          ImGui::SameLine();

          composerView->SetTrackColors(instrumentInstance->instrument, track->GetColorScheme());

          // Track button
          auto trackButtonBegCursor = ImGui::GetCursorPos();
          ImGui::PushID(trackInstance.second.uniqueGuiIdTrackButton.c_str());
          ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
          ConditionalEnableBegin((soloTrackInstance.first == nullptr || isSoloTrack) && !trackInstance.second.mute);
          if (ImGui::Button(track->GetName().c_str(), ImVec2(trackLabelWidth, Globals::kKeyboardKeyHeight))) {
            pendingPlayTrackInstance = { instrumentInstance, trackId };
          }
          ConditionalEnableEnd();
          ImGui::PopStyleVar();
          ImGui::PopID();

          auto trackButtonEndCursor = ImGui::GetCursorPos();

          imGuiStyle.ItemSpacing = defaultItemSpacing;
          memcpy(imGuiStyle.Colors, defaultColors, sizeof(defaultColors));

          // If it's playing, flash it
          float flashPct = 0.0f;
          {
            auto flashTime = playingTrackFlashTimes[0].find(trackId);
            if (flashTime != playingTrackFlashTimes[0].end()) {
              auto pct = static_cast<float>((Globals::currentTime -
                flashTime->second) / Globals::kPlayTrackFlashDuration);
              if (pct >= 1.0f) {
                playingTrackFlashTimes[0].erase(flashTime);
              }
              else {
                flashPct = 1.0f - pct;
              }
            }
          }

          if (flashPct > 0.0f) {
            ImGui::SetCursorPos(trackButtonBegCursor);
            ImGui::FillRect(ImVec2(trackLabelWidth, Globals::kKeyboardKeyHeight),
              (static_cast<uint32>(flashPct * 255.0f) << 24) | 0xFFFFFFFF);
            ImGui::SetCursorPos(trackButtonEndCursor);
          }
        }
      }

      auto songWindowHovered = false;

      // Song lines
      auto parentWindowPos = ImGui::GetWindowPos();
      ImGui::SetNextWindowPos(ImVec2(parentWindowPos.x + trackTotalWidth,
        parentWindowPos.y - ImGui::GetScrollY()));

      ImVec2 notesCanvasSize(trackAndNotesCanvasSize.x - trackTotalWidth - Globals::kScrollBarWidth,
        std::max(ImGui::GetCursorPosY() + Globals::kScrollBarHeight, trackAndNotesCanvasSize.y));

      ImGui::BeginChild("##SongTabNotes",
        notesCanvasSize,
        false,
        ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
      {
        songWindowHovered = ImGui::IsWindowHovered();

        imGuiStyle.ItemSpacing.x = 0.0f;

        // Measure numbers
        auto measureNumberPos = ImGui::GetCursorPos();
        auto beatLineBegY = 0.0f;
        auto oldCursorPos = ImGui::GetCursorPos();
        for (size_t m = 0; m < song->GetNumMeasures(); ++m) {
          ImGui::SetCursorPos(measureNumberPos);
          ImGui::Text(std::to_string(m + 1).c_str());
          beatLineBegY = ImGui::GetCursorPosY();
          measureNumberPos.x += Globals::kFullBeatWidth * song->GetBeatsPerMeasure();
        }
        ImGui::SetCursorPos(oldCursorPos);

        ImGui::NewLine();

        auto beatWidth = Globals::kFullBeatWidth / sequencer.GetSubdivision();
        auto noteCount = song->GetNoteCount();

        const auto& instrumentInstances = song->GetInstrumentInstances();
        for (const auto& instrumentInstance : instrumentInstances) {
          const auto instrument = instrumentInstance->instrument;

          imGuiStyle.ItemSpacing.y = 0.0f;

          float spacing = Globals::kKeyboardKeyHeight;
          if (instrumentInstance != instrumentInstances.front()) {
            spacing += defaultItemSpacing.y;
          }
          ImGui::NewLine(spacing);

          auto& selectedNotesByTrackId = selectedNotesByInstrumentInstance.try_emplace(instrumentInstance).first->second;
          auto& selectingNotesByTrackId = selectingNotesByInstrumentInstance.try_emplace(instrumentInstance).first->second;

          for (auto& trackInstance : instrumentInstance->trackInstances) {
            // Notes (displayed at current beat zoom level)
            uint32 beatStep = song->GetMinNoteValue() / sequencer.GetSubdivision();
            for (size_t beatIndex = 0; beatIndex < song->GetNoteCount(); beatIndex += beatStep) {
              // Bump the first square by 1 to the right so we can draw a 2-pixel beat line
              if (!beatIndex) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 1.0f);
              }

              auto& note = trackInstance.second.noteVector[beatIndex];

              // Draw empty or filled square radio button depending on whether or not it's enabled
              auto buttonBegPos = ImGui::GetCursorPos();
              auto buttonExtent = ImVec2(beatWidth, Globals::kKeyboardKeyHeight);
              if (ImGui::SquareRadioButton(note.uniqueGuiId.c_str(), note.note != nullptr, buttonExtent.x, buttonExtent.y)) {
                // Any click of a note, enabled or not, without SHIFT held down, clears group selection
                if (!(InputState::Get().modState & KMOD_SHIFT)) {
                  // Don't just clear the main map as we have a reference to an entry
                  for (auto& instrumentSelectedNotes : selectedNotesByInstrumentInstance) {
                    instrumentSelectedNotes.second.clear();
                  }
                }

                // Clicking a disabled note enables it; clicking an enabled note selects it
                if (note.note != nullptr) {
                  mapped_set_toggle(selectedNotesByTrackId, trackInstance.first, beatIndex);
                }
                else {
                  pendingToggleNoteInstance = { instrumentInstance, trackInstance.first, beatIndex };
                  if (!(InputState::Get().modState & KMOD_SHIFT)) {
                    mapped_set_toggle(selectedNotesByTrackId, trackInstance.first, beatIndex);
                  }
                }
              }

              if (ImGui::IsItemHovered()) {
                hoveredNoteInstance = { instrumentInstance, trackInstance.first, beatIndex };
              }

              if (note.note != nullptr) {
                // Draw filled note: TODO: Remove this, should draw it once above
                // https://trello.com/c/Ll8dgleN
                {
                  ImVec4 noteColor = kDefaultNoteColor;
                  if (note.note->GetGameIndex() >= 0 && note.note->GetGameIndex() < _countof(kFretColors)) {
                    noteColor = kFretColors[note.note->GetGameIndex()];
                  }
                  ImGui::SetCursorPos(ImVec2(buttonBegPos.x + 1.0f, buttonBegPos.y + 1.0f));
                  ImGui::FillRect(ImVec2(buttonExtent.x - 2.0f,
                    buttonExtent.y - 2.0f), ImGui::ColorConvertFloat4ToU32(noteColor));
                }
                // If we're drag-selecting, update the selected status
                if (dragBox.x >= 0.0f) {
                  glm::vec4 noteBox(buttonBegPos.x, buttonBegPos.y,
                    buttonBegPos.x + buttonExtent.x, buttonBegPos.y + buttonExtent.y);

                  if (dragBox.x <= noteBox.z && noteBox.x <= dragBox.z &&
                    dragBox.w >= noteBox.y && noteBox.w >= dragBox.y) {
                    mapped_set_add(selectingNotesByTrackId, trackInstance.first, beatIndex);
                  }
                  else {
                    mapped_set_remove(selectingNotesByTrackId, trackInstance.first, beatIndex);
                  }
                }

                // Draw selection box around note if selected or selecting
                if (mapped_set_contains(selectedNotesByTrackId, trackInstance.first, beatIndex) ||
                  mapped_set_contains(selectingNotesByTrackId, trackInstance.first, beatIndex)) {
                  ImGui::SetCursorPos(ImVec2(buttonBegPos.x + 1.0f, buttonBegPos.y + 1.0f));
                  ImGui::DrawRect(ImVec2(buttonExtent.x - 2.0f,
                    buttonExtent.y - 2.0f), ImGui::ColorConvertFloat4ToU32(kDragSelectColor));
                }
              }
              ImGui::SameLine();
            }
            ImGui::NewLine();
          }
        }

        ImGui::SetCursorPosY(notesCanvasSize.y);
        auto beatLineEndY = ImGui::GetCursorPosY();

        // Draw the beat demarcation lines
        float cursorPosX = 0.0f;
        for (uint32 m = 0; m < song->GetNumMeasures(); ++m) {
          uint32 c = ImGui::ColorConvertFloat4ToU32(kMeasureDemarcationLineColor);
          for (uint32 b = 0; b < song->GetBeatsPerMeasure(); ++b) {
            ImGui::SetCursorPos(ImVec2(cursorPosX, beatLineBegY));
            ImGui::FillRect(ImVec2(2.0f, beatLineEndY - beatLineBegY), c);
            cursorPosX += Globals::kFullBeatWidth;
            c = ImGui::ColorConvertFloat4ToU32(kBeatDemarcationLineColor);
          }
        }

        // Draw the play line
        cursorPosX = static_cast<float>(Globals::kFullBeatWidth * sequencer.GetClockBeatTime());
        ImGui::SetCursorPos(ImVec2(cursorPosX, beatLineBegY));
        ImGui::FillRect(ImVec2(2, beatLineEndY - beatLineBegY),
          ImGui::ColorConvertFloat4ToU32(kPlayLineColor));

        // Keep it in sight while playing
        if (Sequencer::Get().IsPlaying()) {
          if (cursorPosX > ImGui::GetScrollX() + notesCanvasSize.x) {
            ImGui::SetScrollX(cursorPosX);
          }
        }

        // Drag box
        {
          static ImVec2 mouseDragBeg;

          // On mouse l-press, start the box recording (if we wait for drag to kick in we
          // lose a few pixels)
          if (ImGui::IsMouseClicked(0) && songWindowHovered) {
            // Don't clear notes if they are attempting to add to the set
            if (!(InputState::Get().modState & KMOD_SHIFT)) {
              selectedNotesByInstrumentInstance.clear();
            }

            songWindowClicked = true;

            mouseDragBeg = ImGui::GetMousePos();
            mouseDragBeg -= ImGui::GetWindowPos();

            mouseDragBeg.x += ImGui::GetScrollX();
            mouseDragBeg.y += ImGui::GetScrollY();
          }

          // On mouse release
          if (ImGui::IsMouseReleased(0)) {
            // Merge selecting notes into selected notes
            for (auto& instrumentSelectingNotes : selectingNotesByInstrumentInstance) {
              auto instrumentSelectedNotes = selectedNotesByInstrumentInstance.find(instrumentSelectingNotes.first);

              for (auto& trackSelectingNotes : instrumentSelectingNotes.second) {
                auto trackSelectedNotes = instrumentSelectedNotes->second.find(trackSelectingNotes.first);
                if (trackSelectedNotes != instrumentSelectedNotes->second.end()) {
                  trackSelectedNotes->second.merge(trackSelectingNotes.second);
                }
                else {
                  instrumentSelectedNotes->second.insert(trackSelectingNotes);
                }
              }
            }
            selectingNotesByInstrumentInstance.clear();

            songWindowClicked = false;

            // Clear the drag box
            dragBox = { -1.0f, -1.0f, -1.0f, -1.0f };
          }

          // Update drag box while dragging (if initiating click happened in song window)
          if (ImGui::IsMouseDragging() && songWindowClicked) {
            auto mouseDragCur = ImGui::GetIO().MousePos;

            if (mouseDragCur.x != -FLT_MAX && mouseDragCur.y != -FLT_MAX) {
              mouseDragCur -= ImGui::GetWindowPos();
              mouseDragCur.x += ImGui::GetScrollX();
              mouseDragCur.y += ImGui::GetScrollY();

              dragBox.x = std::min(mouseDragBeg.x, mouseDragCur.x);
              dragBox.y = std::min(mouseDragBeg.y, mouseDragCur.y);
              dragBox.z = std::max(mouseDragBeg.x, mouseDragCur.x);
              dragBox.w = std::max(mouseDragBeg.y, mouseDragCur.y);
            }
          }

          // Render drag box if valid
          auto w = dragBox.z - dragBox.x;
          auto h = dragBox.w - dragBox.y;

          if (w > 0 && h > 0) {
            auto oldCursorPos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(dragBox.x, dragBox.y));
            ImGui::FillRect(ImVec2(static_cast<float>(w),
              static_cast<float>(h)), ImGui::GetColorU32(kDragBoxFillColor));
            ImGui::DrawRect(ImVec2(static_cast<float>(w),
              static_cast<float>(h)), ImGui::GetColorU32(kDragBoxOutlineColor));
            ImGui::SetCursorPos(oldCursorPos);
          }
        }

        ImGui::EndChild();
      } // Song child region      

      ImGui::EndChild();
    } // Track + Song vertical scroling region

    // Bottom tool bar
    {
      imGuiStyle.ItemSpacing.y = defaultItemSpacing.y;

      ImGui::Separator();

      imGuiStyle.ItemSpacing.x = 2.0f;

      // Play/Pause button
      if (sequencer.IsPlaying()) {
        if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(pauseButtonIconTexture), ImVec2(14, 14))) {
          // @Atomic
          sequencer.Pause();
        }
      }
      else {
        if (ImGui::ArrowButtonEx("PlayButton", ImGuiDir_Right, ImVec2(22, 20), 0)) {
          // @Atomic
          sequencer.Play();
        }
      }

      ImGui::SameLine();

      // Stop button
      if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(stopButtonIconTexture), ImVec2(14, 14))) {
        // @Atomic
        sequencer.Stop();
      }

      imGuiStyle.ItemSpacing = defaultItemSpacing;

      // Grid (subdivision)
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      if (ImGui::BeginCombo("Grid", (std::string("1/") +
        std::to_string(sequencer.GetSubdivision())).c_str())) {
        std::vector<uint32> subDivs;

        uint32 subDiv = Globals::kDefaultMinNote;
        while (subDiv >= 2) {
          subDivs.push_back(subDiv);
          subDiv /= 2;
        }

        for (auto revIter = subDivs.rbegin(); revIter != subDivs.rend(); ++revIter) {
          bool isSelected = (sequencer.GetSubdivision() == *revIter);
          if (ImGui::Selectable((std::string("1/") +
            std::to_string(*revIter)).c_str(), isSelected)) {
            pendingSubdivision = *revIter;
          }
          else {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::PopItemWidth();


      // Loop
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      bool localIsLooping = composerView->IsLooping();
      if (ImGui::Checkbox("Loop", &localIsLooping)) {
        // @Atomic
        composerView->SetLooping(localIsLooping);
      }
      ImGui::PopItemWidth();

      // Metronome
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      bool localIsMetronomeOn = composerView->IsMetronomeOn();
      if (ImGui::Checkbox("Metronome", &localIsMetronomeOn)) {
        // @Atomic
        composerView->SetMetronomeOn(localIsMetronomeOn);
      }
      ImGui::PopItemWidth();

      // Master volume
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      float masterVolume = sequencer.GetMasterVolume();
      if (ImGui::SliderFloat("Master", &masterVolume, 0.0f, 1.0f)) {
        // @Atomic
        sequencer.SetMasterVolume(masterVolume);
      }
    } // Bottom toolbar
  } // Sequencer window
}