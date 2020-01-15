#include "InstrumentTab.h"
#include "Sequencer.h"
#include "Globals.h"
#include "InstrumentBank.h"
#include "Song.h"
#include "OddsAndEnds.h"
#include "DialogTrack.h"
#include "DialogInstrument.h"
#include "ComposerView.h"

#include "SDL.h"
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

static constexpr float kInstrumentLabelWidth = 200.0f;

InstrumentTab::InstrumentTab(ComposerView* composerView)
  : View("Instrument", nullptr)
  , composerView(composerView) {

}

InstrumentTab::~InstrumentTab() {
  // Delete any instruments we haven't saved yet
  for (auto& newInstrument : newInstruments) {
    delete newInstrument;
  }
  newInstruments.clear();
}

void InstrumentTab::Show() {

}

void InstrumentTab::Hide() {

}

std::string InstrumentTab::GetUniqueInstrumentName(std::string instrumentNameBase) {
  // Pick an available name
  std::string instrumentName = instrumentNameBase;

  // Has to end sometime
  const auto& savedInstruments = InstrumentBank::Get().GetInstruments();
  for (int nameSuffix = 1; nameSuffix < 1000; ++nameSuffix) {
    auto savedInstrumentIter = savedInstruments.begin();
    while (savedInstrumentIter != savedInstruments.end()) {
      if (savedInstrumentIter->second->GetName() == instrumentName) {
        break;
      }
      ++savedInstrumentIter;
    }

    if (savedInstrumentIter == savedInstruments.end()) {
      auto newInstrumentIter = newInstruments.begin();
      while (newInstrumentIter != newInstruments.end()) {
        if ((*newInstrumentIter)->GetName() == instrumentName) {
          break;
        }
        ++newInstrumentIter;
      }

      if (newInstrumentIter == newInstruments.end()) {
        break;
      }
    }

    instrumentName = std::string(instrumentNameBase) +
      std::string(" (") + std::to_string(nameSuffix) + std::string(")");
  }

  return instrumentName;
}

bool InstrumentTab::SaveInstrument(Instrument* instrument) {
  WCHAR szFile[FILENAME_MAX] = { 0 };
  OPENFILENAME ofn = { 0 };

  USES_CONVERSION;
  std::string windowTitle("Save Instrument");
  ofn.lpstrTitle = A2W(windowTitle.c_str());
  ofn.hwndOwner = reinterpret_cast<HWND>(Globals::mainWindowHandle);
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
  ofn.lpstrFilter = _TEXT("JSON\0*.json\0");
  ofn.nFilterIndex = 0;
  ofn.Flags = OFN_OVERWRITEPROMPT;

  if (GetSaveFileName(&ofn)) {
    return instrument->SaveInstrument(W2A(szFile));
  }
  return false;
}

std::string InstrumentTab::GetUniqueTrackName(Instrument* instrument, std::string trackNameBase) {
  // Pick an available name
  std::string trackName = trackNameBase;

  // Has to end sometime
  for (int nameSuffix = 1; nameSuffix < 1000; ++nameSuffix) {
    const auto& tracks = instrument->GetTracks();

    auto trackIter = tracks.begin();
    while (trackIter != tracks.end()) {
      if (trackIter->second->GetName() == trackName) {
        break;
      }
      ++trackIter;
    }

    if (trackIter == tracks.end()) {
      break;
    }

    trackName = std::string(trackNameBase) +
      std::string(" (") + std::to_string(nameSuffix) + std::string(")");
  }

  return trackName;
}

void InstrumentTab::DoLockedActions() {
  auto& sequencer = Sequencer::Get();

  // Track volume changed
  if (pendingVolumeTrack.instrument != nullptr) {
    auto track = pendingVolumeTrack.instrument->GetTrackById(pendingVolumeTrack.trackId);
    if (track != nullptr) {
      track->SetVolume(pendingVolumeTrack.data);
    }
  }

  // Add voice
  if (pendingPlayTrack.instrument != nullptr) {
    auto track = pendingPlayTrack.instrument->GetTrackById(pendingPlayTrack.trackId);
    if (track != nullptr) {
      sequencer.PlayPatch(track->GetPatch(), track->GetVolume());
    }
  }

  // Track cloned via dialog previous frame
  if (pendingCloneTrack.instrument != nullptr) {
    auto oldTrack = pendingCloneTrack.instrument->GetTrackById(pendingCloneTrack.trackId);
    assert(oldTrack != nullptr);

    auto newTrack = new Track(*oldTrack);

    newTrack->SetName(GetUniqueTrackName(pendingCloneTrack.instrument, oldTrack->GetName()));

    // Add track to instrument instance
    pendingCloneTrack.instrument->AddTrack(newTrack);
  }

  // Track removed via dialog previous frame
  // NOTE: This is the last delayed track operation just in case
  if (pendingRemoveTrack.instrument!= nullptr) {
    // Might be nice to let the other voices continue, but a lot of things would be nice
    bool wasPlaying = sequencer.IsPlaying();
    if (wasPlaying) {
      sequencer.PauseKill();
    }

    // Remove track from instrument instance
    pendingRemoveTrack.instrument->RemoveTrackById(pendingRemoveTrack.trackId);

    if (wasPlaying) {
      sequencer.Play();
    }
  }

  if (pendingNewInstrument) {
    auto newInstrument = new Instrument(GetUniqueInstrumentName(Instrument::kDefaultName));
    assert(newInstrument);
    newInstruments.insert(newInstrument);
    
    auto instrumentInstance = newInstrument->Instance();
    instrumentInstances.insert({ newInstrument, instrumentInstance });

    openInstruments.insert(newInstrument);
  }

  if (pendingLoadInstrument) {
    auto instrument = InstrumentBank::Get().LoadInstrumentName({ }, false);
    if (instrument != nullptr) {
      auto instrumentInstance = instrument->Instance();
      instrumentInstances.insert({ instrument, instrumentInstance });
      openInstruments.insert(instrument);
    }
  }

  if (pendingSaveInstrument != nullptr) {
    pendingSaveInstrument->Save();
  }

  if (pendingSaveAsInstrument != nullptr) {
    if (SaveInstrument(pendingSaveAsInstrument)) {
      auto newInstrument = newInstruments.find(pendingSaveAsInstrument);
      if (newInstrument != newInstruments.end()) {
        newInstruments.erase(newInstrument);
      }
    }
  }

  // Reset all pendings
  pendingVolumeTrack = { };
  pendingPlayTrack = { };
  pendingCloneTrack = { };
  pendingRemoveTrack = { };
  pendingNewInstrument = false;
  pendingLoadInstrument = false;
  pendingSaveInstrument = nullptr;
  pendingSaveAsInstrument = nullptr;
}

void InstrumentTab::ConditionalEnableBegin(bool condition) {
  localGuiDisabled = !condition;
  if (localGuiDisabled) {
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
  }
}

void InstrumentTab::ConditionalEnableEnd() {
  if (localGuiDisabled) {
    ImGui::PopStyleVar();
    ImGui::PopItemFlag();
  }
  localGuiDisabled = false;
}

void InstrumentTab::DoMainMenuBar() {
  if (ImGui::BeginMenu("Instrument")) {
    if (ImGui::MenuItem("New")) {
      pendingNewInstrument = true;
    }
    if (ImGui::MenuItem("Load")) {
      pendingLoadInstrument = true;
    }
    ImGui::EndMenu();
  }
}

void InstrumentTab::Render(ImVec2 canvasSize) {
  auto& sequencer = Sequencer::Get();
  auto imGuiFont = ImGui::GetFont();
  auto& imGuiStyle = ImGui::GetStyle();
  auto defaultItemSpacing = imGuiStyle.ItemSpacing;
  auto defaultFramePadding = imGuiStyle.FramePadding;

  static constexpr float kTopToolbarHeight = 26.0f;
  static constexpr float kBottomToolbarHeight = 50.0f;

  ImVec4 defaultColors[ImGuiCol_COUNT];
  memcpy(defaultColors, imGuiStyle.Colors, sizeof(defaultColors));

  static constexpr float kTopSpace = 46.0f;
  static ImVec2 leftWindowPos(0.0f, kTopSpace);

  // Gather any new instruments
  auto& instruments = InstrumentBank::Get().GetInstruments();
  for (auto& instrument : instruments) {
    auto instanceEntry = instrumentInstances.find(instrument.second);
    if (instanceEntry != instrumentInstances.end()) {
      continue;
    }
    instrumentInstances.insert({ instrument.second, instrument.second->Instance() });
  }

  ImGui::BeginChild("##SongTabNotes",
    canvasSize,
    false,
    ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
  {
    // Left-most column is list of open instruments
    
    // Columns are so broke-ass ... have to have at least two to use the column
    // functions.
    ImGui::Columns(std::max(openInstruments.size(), 1u) + 1, "InstrumentColumns");
    ImGui::Text("Instruments");
    ImGui::NextColumn();

    if (openInstruments.empty()) {
      ImGui::NextColumn();
    }

    static const auto kUnsavedInstrumentColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

    // Render instrument names as top row; gather iterators
    Instrument* pendingCloseInstrument = nullptr;
    Instrument* pendingOpenInstrument = nullptr;
    std::vector<std::tuple<bool,
      Instrument*,
      std::map<uint32, TrackInstance>::const_iterator,
      std::map<uint32, TrackInstance>::const_iterator>> instrumentTrackIters;
    for (auto& instrumentInstance : instrumentInstances) {
      // If it's open
      if (openInstruments.find(instrumentInstance.second->instrument) != openInstruments.end()) {

        // Visual cue for unsaved instruments
        if (newInstruments.find(instrumentInstance.second->instrument) != newInstruments.end()) {
          imGuiStyle.Colors[ImGuiCol_Text] = kUnsavedInstrumentColor;
        }

        ImGui::Text(instrumentInstance.second->instrument->GetName().c_str());
        imGuiStyle.Colors[ImGuiCol_Text] = defaultColors[ImGuiCol_Text];
        ImGui::SameLine(ImGui::GetColumnWidth() - 26.0f);
        if (ImGui::SmallButton("x")) {
          pendingCloseInstrument = instrumentInstance.second->instrument;
        }
        ImGui::NextColumn();

        const auto& trackInstances = instrumentInstance.second->trackInstances;
        instrumentTrackIters.push_back({ true, instrumentInstance.second->instrument, trackInstances.begin(), trackInstances.end() });
      }
      else {
        const auto& trackInstances = instrumentInstance.second->trackInstances;
        instrumentTrackIters.push_back({ false, instrumentInstance.second->instrument, trackInstances.end(), trackInstances.end() });
      }
    }

    auto instrumentInstanceIter = std::make_pair(instrumentInstances.begin(), instrumentInstances.end());

    ImGui::Separator();

    imGuiStyle.ItemSpacing.y = 0.0f;

    while (true) {
      bool shouldContinue = false;

      // Possibly draw an entry in the instrument list
      if (instrumentInstanceIter.first != instrumentInstanceIter.second) {
        shouldContinue |= true;

        imGuiStyle.ItemSpacing.x = 0.0f;

        // Instrument hamburger menu
        ImGui::PushID(instrumentInstanceIter.first->second->uniqueGuiIdHamburgerMenu.c_str());
        if (ImGui::Button("=", ImVec2(Globals::kHamburgerMenuWidth, Globals::kKeyboardKeyHeight))) {
          ImGui::PopID();
          ImGui::OpenPopup(instrumentInstanceIter.first->second->uniqueGuiIdPropertiesPop.c_str());
        }
        else {
          ImGui::PopID();
        }

        ImGui::SameLine();

        imGuiStyle.ItemSpacing.x = defaultItemSpacing.x;

        memcpy(imGuiStyle.Colors, defaultColors, sizeof(defaultColors));

        imGuiStyle.ItemSpacing.y = defaultItemSpacing.y;
        if (ImGui::BeginPopup(instrumentInstanceIter.first->second->uniqueGuiIdPropertiesPop.c_str())) {
          bool closePopup = false;

          if (ImGui::MenuItem("Add Track")) {
            composerView->ShowDialog(new DialogTrack("Add Track",
              instrumentInstanceIter.first->second->instrument, -1,
              new Track(GetUniqueTrackName(instrumentInstanceIter.first->second->instrument,
                Globals::kDefaultNewTrackName))));
          }

          ConditionalEnableBegin(!instrumentInstanceIter.first->second->instrument->GetFileName().empty());
          if (ImGui::MenuItem("Save")) {
            pendingSaveInstrument = instrumentInstanceIter.first->second->instrument;
          }
          ConditionalEnableEnd();

          if (ImGui::MenuItem("Save As")) {
            pendingSaveAsInstrument = instrumentInstanceIter.first->second->instrument;
          }

          ConditionalEnableBegin(openInstruments.find(instrumentInstanceIter.
            first->second->instrument) == openInstruments.end());
          if (ImGui::MenuItem("View")) {
            pendingOpenInstrument = instrumentInstanceIter.first->second->instrument;
          }
          ConditionalEnableEnd();

          if (ImGui::MenuItem("Properties")) {
            composerView->ShowDialog(new DialogInstrument("Instrument",
              instrumentInstanceIter.first->second->instrument));
            closePopup = true;
          }

          if (closePopup) {
            ImGui::CloseCurrentPopup();
          }
          ImGui::EndPopup();
        }
        imGuiStyle.ItemSpacing.y = 0.0f;

        // Visual cue for unsaved instruments
        if (instrumentInstanceIter.first->second->instrument->GetFileName().empty()) {
          imGuiStyle.Colors[ImGuiCol_Text] = kUnsavedInstrumentColor;
        }
        char newInstrumentName[256] = { 0 };
        strcpy(newInstrumentName, instrumentInstanceIter.first->second->instrument->GetName().c_str());
        ImGui::PushID(instrumentInstanceIter.first->second->uniqueGuiIdName.c_str());
        if (ImGui::InputTextEx("", nullptr, newInstrumentName,
          _countof(newInstrumentName) - 1, ImVec2(ImGui::GetColumnWidth(), Globals::kKeyboardKeyHeight), 0)) {
          instrumentInstanceIter.first->second->instrument->SetName(std::string(newInstrumentName));
        }
        ImGui::PopID();
        imGuiStyle.Colors[ImGuiCol_Text] = defaultColors[ImGuiCol_Text];

        ++instrumentInstanceIter.first;
      }
      ImGui::NextColumn();

      if (openInstruments.empty()) {
        ImGui::NextColumn();
      }
      else {
        for (auto& trackIter : instrumentTrackIters) {
          if (std::get<0>(trackIter)) {
            auto& i = std::get<1>(trackIter);
            auto& c = std::get<2>(trackIter);
            auto& e = std::get<3>(trackIter);
            if (c != e) {
              shouldContinue |= true;

              auto t = i->GetTrackById(c->second.trackId);

              imGuiStyle.ItemSpacing.x = 0.0f;

              composerView->SetTrackColors(i, t->GetColorScheme());

              // Track hamburger menu
              ImGui::PushID(c->second.uniqueGuiIdHamburgerMenu.c_str());
              if (ImGui::Button("=", ImVec2(Globals::kHamburgerMenuWidth, Globals::kKeyboardKeyHeight))) {
                ImGui::PopID();
                ImGui::OpenPopup(c->second.uniqueGuiIdPropertiesPop.c_str());
              }
              else {
                ImGui::PopID();
              }
              memcpy(imGuiStyle.Colors, defaultColors, sizeof(defaultColors));

              ImGui::SameLine();

              imGuiStyle.ItemSpacing.x = defaultItemSpacing.x;

              memcpy(imGuiStyle.Colors, defaultColors, sizeof(defaultColors));

              imGuiStyle.ItemSpacing.y = defaultItemSpacing.y;
              if (ImGui::BeginPopup(c->second.uniqueGuiIdPropertiesPop.c_str())) {
                bool closePopup = false;

                if (ImGui::MenuItem("Clone")) {
                  pendingCloneTrack = { i, c->second.trackId };
                  closePopup = true;
                }

                if (ImGui::MenuItem("Remove")) {
                  pendingRemoveTrack = { i, c->second.trackId };
                  closePopup = true;
                }

                if (ImGui::MenuItem("Properties")) {
                  composerView->ShowDialog(new DialogTrack("Edit Track",
                    i, c->second.trackId, new Track(*t)));
                  closePopup = true;
                }

                if (closePopup) {
                  ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
              }
              imGuiStyle.ItemSpacing.y = 0.0f;

              char newTrackName[256] = { 0 };

              composerView->SetTrackColors(i, t->GetColorScheme());

              strcpy(newTrackName, t->GetName().c_str());
              ImGui::PushID(c->second.uniqueGuiIdTrackButton.c_str());
              ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
              if (ImGui::Button(t->GetName().c_str(), ImVec2(-1, Globals::kKeyboardKeyHeight))) {
                pendingPlayTrack = { i, c->second.trackId };
              }
              ImGui::PopStyleVar();
              ImGui::PopID();

              imGuiStyle.ItemSpacing = defaultItemSpacing;
              memcpy(imGuiStyle.Colors, defaultColors, sizeof(defaultColors));
              ++c;
            }
            ImGui::NextColumn();
          }
        }
      }

      if (!shouldContinue) {
        break;
      }
    }

    imGuiStyle.ItemSpacing.y = defaultItemSpacing.y;

    if (!openInstruments.empty()) {
      ImGui::Separator();
    }

    if (pendingCloseInstrument != nullptr) {
      openInstruments.erase(pendingCloseInstrument);
    }
    if (pendingOpenInstrument != nullptr) {
      openInstruments.insert(pendingOpenInstrument);
    }
    ImGui::End();
  }
#if 0
  const char* names[3] = { "One", "Two", "Three" };
  const char* paths[3] = { "/path/one", "/path/two", "/path/three" };
  static int selected = -1;
  for (int i = 0; i < 3; i++)
  {
    char label[32];
    sprintf(label, "%04d", i);
    if (ImGui::Selectable(label, selected == i, ImGuiSelectableFlags_SpanAllColumns))
      selected = i;
    bool hovered = ImGui::IsItemHovered();
    ImGui::NextColumn();
    ImGui::Text(names[i]); ImGui::NextColumn();
    ImGui::Text(paths[i]); ImGui::NextColumn();
    ImGui::Text("%d", hovered); ImGui::NextColumn();
  }
#endif

#if 0
  // Left window: loaded instrument list
  ImVec2 instrumentListCanvasSize(kInstrumentLabelWidth + Globals::kScrollBarWidth, canvasSize.y - 9.0f);
  ImGui::SetNextWindowSize(instrumentListCanvasSize);
  ImGui::SetNextWindowPos(leftWindowPos);
  ImGui::Begin("Instruments",
    nullptr,
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);// | ImGuiWindowFlags_AlwaysVerticalScrollbar);
  {
    memcpy(imGuiStyle.Colors, defaultColors, sizeof(defaultColors));

    auto labelWidth = Globals::kKeyboardKeyHeight - Globals::kHamburgerMenuWidth - defaultItemSpacing.x;

    for (auto& instrumentData : instrumentDataList) {
      if (instrumentData.instrument != instrumentDataList.front().instrument) {
        ImGui::Separator();
      }

      // Instrument hamburger menu
      ImGui::PushID(instrumentData.uniqueGuiIdHamburgerMenu.c_str());
      if (ImGui::Button("=", ImVec2(Globals::kHamburgerMenuWidth, Globals::kKeyboardKeyHeight))) {
        ImGui::PopID();
        ImGui::OpenPopup(instrumentData.uniqueGuiIdPropertiesPop.c_str());
      }
      else {
        ImGui::PopID();
      }
      ImGui::SameLine();

      if (ImGui::BeginPopup(instrumentData.uniqueGuiIdPropertiesPop.c_str())) {
        bool closePopup = false;

        if (ImGui::MenuItem("View")) {
          instrumentData.isOpen = true;
          openedInstrument = instrumentData.instrument;
          closePopup = true;
        }

        if (closePopup) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      char newInstrumentName[256] = { 0 };
      strcpy(newInstrumentName, instrumentData.instrument->GetName().c_str());
      ImGui::PushID((std::string("lst: ") + std::to_string(reinterpret_cast<uint32>(instrumentData.instrument))).c_str());
      if (ImGui::InputTextEx("", nullptr, newInstrumentName,
        _countof(newInstrumentName) - 1, ImVec2(labelWidth, Globals::kKeyboardKeyHeight), 0)) {
        instrumentData.instrument->SetName(std::string(newInstrumentName));
      }
      ImGui::PopID();
    }

    ImGui::End();
  }

  // Right window: instrumentDataList
  //ImVec2 instrumentsCanvasSize(canvasSize.x - instrumentListCanvasSize.x, canvasSize.y);
  //ImGui::SetNextWindowSize(instrumentListCanvasSize);
  //ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 200.0f), ImVec2(800.0f, 800.0f));
  for (auto& instrumentData : instrumentDataList) {
    if (instrumentData.isOpen) {
      if (openedInstrument == instrumentData.instrument) {
        ImGui::SetNextWindowFocus();
      }
      if (ImGui::Begin(instrumentData.instrument->GetName().c_str(),
        &instrumentData.isOpen,
        0)) {
        for (const auto& track : instrumentData.instrument->GetTracks()) {
          ImGui::Text(track.second->GetName().c_str());
        }
        ImGui::End();
      }
    }
  }
#endif
}
