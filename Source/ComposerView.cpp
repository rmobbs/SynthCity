#include "ComposerView.h"

#include <vector>
#include <string_view>
#include <algorithm>
#include <mutex>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuiExtensions.h"
#include "SDL.h"
#include "soil.h"
#include "Logging.h"
#include "Sequencer.h"
#include "AudioGlobals.h"
#include "GlobalRenderData.h"
#include "ShaderProgram.h"
#include "Instrument.h"
#include "DialogTrack.h"
#include "Patch.h"
#include "ProcessDecay.h"
#include "WavSound.h"
#include "Globals.h"
#include "InputState.h"
#include "GamePreviewView.h"
#include "Song.h"
#include "OddsAndEnds.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
#include <commdlg.h>
#include <atlbase.h>
#include <commctrl.h>

static constexpr float kFullBeatWidth = 80.0f;
static constexpr float kKeyboardKeyHeight = 20.0f;
static constexpr uint32 kPlayTrackFlashColor = 0x00007F7F;
static constexpr float kPlayTrackFlashDuration = 0.5f;
static constexpr float kOutputWindowWindowScreenHeightPercentage = 0.35f;
static constexpr float kSequencerWindowToolbarHeight = 74.0f;
static constexpr float kHamburgerMenuWidth(20.0f);
static constexpr std::string_view kJsonTag(".json");
static constexpr const char* kDefaultNewTrackName("NewTrack");
static constexpr const char* kTrackNameFormat("XXXXXXXXXXXXXXXX");
static constexpr const char* kModeStrings[] = {
  "Normal",
  "Markup",
};

static const ImVec4 kDefaultNoteColor(1.0f, 1.0f, 1.0f, 0.5f);
static const ImVec4 kDragBoxColor(1.0f, 1.0f, 1.0f, 1.0f);
static const ImVec4 kDragSelectColor(1.0f, 1.0f, 0.0f, 1.0f);
static const ImVec4 kFretColors[] = {
  ImVec4(1.0f, 0.0f, 0.0f, 0.5f),
  ImVec4(0.0f, 1.0f, 0.0f, 0.5f),
  ImVec4(0.0f, 0.0f, 1.0f, 0.5f),
  ImVec4(1.0f, 1.0f, 0.0f, 0.5f),
};

static const ImVec4 kMeasureDemarcationLineColor(1.0f, 1.0f, 1.0f, 1.0f);
static const ImVec4 kBeatDemarcationLineColor(0.6f, 0.6f, 0.6f, 1.0f);
static const ImVec4 kPlayLineColor(1.0f, 1.0f, 0.0f, 1.0f);

void ComposerView::OutputWindowState::ClearLog() {
  displayHistory.clear();
}

void ComposerView::OutputWindowState::AddLog(const char* fmt, ...)
{
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
  buf[IM_ARRAYSIZE(buf) - 1] = 0;
  va_end(args);
  this->displayHistory.emplace_back(buf);
  if (autoScroll) {
    scrollToBottom = true;
  }
}

void ComposerView::OutputWindowState::AddLog(const std::string_view& logString) {
  displayHistory.push_back(std::string(logString));
  if (autoScroll) {
    scrollToBottom = true;
  }
}

bool LoadInstrument(HWND mainWindowHandle, std::string instrumentName = {}) {
  WCHAR szFile[FILENAME_MAX] = { 0 };
  OPENFILENAME ofn = { 0 };

  USES_CONVERSION;
  ofn.lStructSize = sizeof(ofn);

  std::string windowTitle("Open instrument");
  if (instrumentName.length() != 0) {
    windowTitle += " \'" + instrumentName + "\'";
  }
  ofn.lpstrTitle = A2W(windowTitle.c_str());
  ofn.hwndOwner = mainWindowHandle;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
  ofn.lpstrFilter = _TEXT("JSON\0*.json\0");
  ofn.nFilterIndex = 0;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileName(&ofn)) {
    if (Sequencer::Get().LoadInstrument(std::string(W2A(szFile)), instrumentName)) {
      return true;
    }
  }
  return false;
}

bool SaveInstrument() {
  WCHAR szFile[FILENAME_MAX] = { 0 };
  OPENFILENAME ofn = { 0 };

  USES_CONVERSION;
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
  ofn.lpstrFilter = _TEXT("JSON\0*.json\0");
  ofn.nFilterIndex = 0;
  ofn.Flags = OFN_OVERWRITEPROMPT;

  if (GetSaveFileName(&ofn)) {
    std::string fileName(W2A(szFile));

    if (fileName.compare(fileName.length() - kJsonTag.length(), kJsonTag.length(), kJsonTag)) {
      fileName += kJsonTag;
    }
    Sequencer::Get().GetInstrument()->SaveInstrument(fileName);
  }
  return false;
}

bool LoadSong() {
  WCHAR szFile[FILENAME_MAX] = { 0 };
  OPENFILENAME ofn = { 0 };

  USES_CONVERSION;
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
  // TODO: Fix MIDI loading
  // https://trello.com/c/vQCRzrcm
  ofn.lpstrFilter = _TEXT("JSON\0*.json\0");//MIDI\0 * .midi; *.mid\0");
  ofn.nFilterIndex = 0;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileName(&ofn)) {
    Sequencer::Get().LoadSong(std::string(W2A(szFile)));
  }
  return true;
}

bool SaveSong() {
  if (Sequencer::Get().GetInstrument() != nullptr) {
    WCHAR szFile[FILENAME_MAX] = { 0 };
    OPENFILENAME ofn = { 0 };

    USES_CONVERSION;
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = _TEXT("JSON\0*.json\0");
    ofn.nFilterIndex = 0;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn)) {
      std::string fileName(W2A(szFile));

      if (fileName.compare(fileName.length() - kJsonTag.length(), kJsonTag.length(), kJsonTag)) {
        fileName += kJsonTag;
      }
      Sequencer::Get().SaveSong(fileName);
    }
  }

  return true;
}

std::string GetNewTrackName(const std::string& trackNameBase) {
  // Pick an available name
  std::string trackName = trackNameBase;
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

    trackName = std::string(trackNameBase) + std::string(" (") + std::to_string(nameSuffix) + std::string(")");
  }
  return trackName;
}

// TODO: Better implementation of custom track color schemes
// https://trello.com/c/VX59Thk1
void ComposerView::SetTrackColors(std::string colorScheme, uint32& flashColor) {

  if (colorScheme.length()) {
    auto& imGuiStyle = ImGui::GetStyle();

    std::transform(colorScheme.begin(), colorScheme.end(), colorScheme.begin(), ::tolower);

    if (colorScheme == "piano:white") {
      imGuiStyle.Colors[ImGuiCol_Button] = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
      imGuiStyle.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.4f, 0.4f, 0.4f, 0.5f);
      imGuiStyle.Colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
      flashColor = 0x00666680;
    }
    if (colorScheme == "piano:black") {
      imGuiStyle.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
      imGuiStyle.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.7f, 0.7f, 0.7f, 0.5f);
      imGuiStyle.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
      flashColor = 0x00838380;
    }
  }
}

void ComposerView::SelectedGroupAction(std::function<void(int32, int32)> action) {
  for (size_t trackIndex = 0; trackIndex < noteSelectedStatus.size(); ++trackIndex) {
    for (auto& val : noteSelectedStatus[trackIndex]) {
      action(trackIndex, val);
    }
  }
}

void ComposerView::HandleInput() {
  auto& inputState = InputState::Get();

  if (inputState.pressed[SDLK_DELETE]) {
    SelectedGroupAction([](int32 lineIndex, int32 noteIndex) {
      auto& note = Sequencer::Get().GetSong()->GetLine(lineIndex)[noteIndex];
      note.SetEnabled(false);
    });

    ClearSelectedNotes();
  }

  if (inputState.pressed[SDLK_ESCAPE]) {
    ClearSelectedNotes();
  }

  if (inputState.pressed[SDLK_SPACE]) {
    if (Sequencer::Get().IsPlaying()) {
      Sequencer::Get().Stop();
    }
    else {
      Sequencer::Get().Play();
    }
  }

  if (inputState.modState & KMOD_CTRL) {
    // Copy
    if (inputState.pressed[SDLK_c]) {
      // Don't clear the existing clipboard if we have nothing currently selected, as
      // accidentally pressing CTLR+C again after CTRL+C (instead of CTRL+V) is very
      // frustrating
      bool notesSelected = false;
      for (const auto& nss : noteSelectedStatus) {
        if (nss.size() > 0) {
          notesSelected = true;
          break;
        }
      }

      if (notesSelected) {
        noteClipboard = noteSelectedStatus;
      }
    }

    // Paste
    else if (inputState.pressed[SDLK_v]) {
      if (noteClipboard.size()) {
        auto song = Sequencer::Get().GetSong();
        assert(song != nullptr);

        // We need to know the first note to generate the proper offset
        size_t firstNote = UINT32_MAX;
        for (const auto& ncp : noteClipboard) {
          if (ncp.empty()) {
            continue;
          }

          // Ordered set
          auto iter = ncp.begin();
          if (firstNote > *iter) {
            firstNote = *iter;
          }
        }

        if (firstNote != UINT32_MAX) {
        }

      }
    }
  }

  switch (mode) {
    case Mode::Normal: {
      if ((inputState.modState & KMOD_CTRL) && inputState.pressed[SDLK_m]) {
        mode = Mode::Markup;
      }
      break;
    }
    case Mode::Markup: {
      // Mode toggle
      if (inputState.modState & KMOD_CTRL) {
        if (inputState.pressed[SDLK_m]) {
          mode = Mode::Normal;
        }
      }
      else {
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
          SelectedGroupAction([newGameIndex](int32 lineIndex, int32 noteIndex) {
            auto& note = Sequencer::Get().GetSong()->GetLine(lineIndex)[noteIndex];
            note.SetGameIndex(newGameIndex);
          });
        }
      }
      break;
    }
  }

  hoveredNote = { -1, -1 };
}

void ComposerView::ProcessPendingActions() {
  auto& sequencer = Sequencer::Get();

  // Newly triggered notes are written to entry 1 in the audio callback
  playingTrackFlashTimes[0].merge(playingTrackFlashTimes[1]);

  // Subdivision changed
  if (pendingSubdivision != -1) {
    sequencer.SetSubdivision(pendingSubdivision);
  }

  // Beats per minute changed
  if (pendingTempo != -1) {
    sequencer.SetTempo(pendingTempo);
  }

  // Master volume changed
  if (pendingMasterVolume >= 0.0f) {
    Mixer::Get().SetMasterVolume(pendingMasterVolume);
  }

  auto instrument = sequencer.GetInstrument();
  if (instrument != nullptr) {
    // Track volume changed
    if (pendingTrackVolume.first != -1) {
      auto track = instrument->GetTrack(pendingTrackVolume.first);
      if (track != nullptr) {
        track->SetVolume(pendingTrackVolume.second);
      }
    }

    // Track mute state changed
    if (pendingTrackMute.first != -1) {
      auto track = instrument->GetTrack(pendingTrackMute.first);
      if (track != nullptr) {
        track->SetMute(pendingTrackMute.second);
      }
    }

    // Track spawns voice
    if (pendingPlayTrack != -1) {
      instrument->PlayTrack(pendingPlayTrack);
    }

    // Mute all voices except the solo track
    if (pendingSoloTrack != -2) {
      instrument->SetSoloTrack(pendingSoloTrack);
    }

    // Toggle a note on/off
    if (toggledNote.second != -1) {
      auto& note = Sequencer::Get().GetSong()->GetLine(toggledNote.first)[toggledNote.second];
      note.SetEnabled(!note.GetEnabled());
    }

    // Change a note's fret index

    // Track cloned via dialog previous frame
    if (pendingCloneTrack != -1) {
      auto oldTrack = instrument->GetTrack(pendingCloneTrack);
      if (oldTrack != nullptr) {
        auto newTrack = new Track(*oldTrack);
        newTrack->SetName(GetNewTrackName(oldTrack->GetName()));
        instrument->AddTrack(newTrack);
        Sequencer::Get().GetSong()->AddLine();
      }
    }

    // Track removed via dialog previous frame
    // NOTE: This is the last delayed track operation just in case
    if (pendingRemoveTrack != -1) {
      noteSelectedStatus.erase(noteSelectedStatus.begin() + pendingRemoveTrack);
      instrument->RemoveTrack(pendingRemoveTrack);
      Sequencer::Get().GetSong()->RemoveLine(pendingRemoveTrack);
    }
  }

  if (pendingNewInstrument) {
    ClearSelectedNotes();
    sequencer.NewInstrument();
  }

  if (pendingLoadInstrument) {
    ClearSelectedNotes();
    LoadInstrument(reinterpret_cast<HWND>(mainWindowHandle));
  }

  if (pendingSaveInstrument) {
    SaveInstrument();
  }

  if (pendingNewSong) {
    ClearSelectedNotes();
    sequencer.NewSong();
  }

  if (pendingLoadSong) {
    ClearSelectedNotes();
    LoadSong();
  }

  if (pendingSaveSong) {
    SaveSong();
  }

  // Reset all pendings
  pendingSubdivision = -1;
  pendingTempo = -1;
  pendingMasterVolume = -1.0f;
  pendingTrackVolume = { -1, 0.0f };
  pendingTrackMute = { -1, false };
  pendingPlayTrack = -1;
  pendingRemoveTrack = -1;
  pendingCloneTrack = -1;
  pendingSoloTrack = -2;
  toggledNote = { -1, -1 };
  pendingNewInstrument = false;
  pendingLoadInstrument = false;
  pendingSaveInstrument = false;
  pendingNewSong = false;
  pendingLoadSong = false;
  pendingSaveSong = false;
}

void ComposerView::ClearSelectedNotes() {
  for (auto& nss : noteSelectedStatus) {
    nss.clear();
  }
}

void ComposerView::Render(ImVec2 canvasSize) {
  auto& sequencer = Sequencer::Get();

  auto curFrame = Mixer::Get().GetCurFrame();

  glClearColor(0.5f, 0.5f, 0.5f, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  AudioGlobals::LockAudio();
  ProcessPendingActions();
  HandleInput();
  AudioGlobals::UnlockAudio();

  ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(canvasSize.x), static_cast<float>(canvasSize.y));
  ImGui::NewFrame();

  auto mainMenuBarHeight = 0.0f;
  if (ImGui::BeginMainMenuBar()) {
    mainMenuBarHeight = ImGui::GetWindowSize().y;

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Instrument")) {
        pendingNewInstrument = true;
      }
      if (ImGui::MenuItem("Load Instrument")) {
        pendingLoadInstrument = true;
      }
      if (ImGui::MenuItem("Save Instrument")) {
        pendingSaveInstrument = true;
      }
      if (ImGui::MenuItem("Exit")) {
        SDL_PushEvent(&SDL_Event({ SDL_QUIT }));
      }
      ImGui::EndMenu();
    }
    if (sequencer.GetInstrument() != nullptr) {
      if (ImGui::BeginMenu("Instrument")) {
        if (ImGui::MenuItem("Add Track")) {
          pendingDialog = new DialogTrack("Add Track", -1, new Track(GetNewTrackName(kDefaultNewTrackName)), playButtonIconTexture, stopButtonIconTexture);
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Song")) {
        if (ImGui::MenuItem("New Song")) {
          pendingNewSong = true;
        }
        if (ImGui::MenuItem("Load Song")) {
          pendingLoadSong = true;
        }
        if (ImGui::MenuItem("Save Song")) {
          pendingSaveSong = true;
        }
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Game")) {
        if (ImGui::MenuItem("Preview")) {
          sequencer.Stop();
          View::SetCurrentView<GamePreviewView>();
        }
        ImGui::EndMenu();
      }
    }
    ImGui::EndMainMenuBar();
  }

  if (pendingDialog != nullptr) {
    wasPlaying = Sequencer::Get().IsPlaying();

    Sequencer::Get().PauseKill();

    assert(activeDialog == nullptr);
    activeDialog = pendingDialog;
    activeDialog->Open();
    pendingDialog = nullptr;
  }

  if (activeDialog != nullptr) {
    if (!activeDialog->Render()) {
      delete activeDialog;
      activeDialog = nullptr;

      if (wasPlaying) {
        Sequencer::Get().Play();
      }
    }
  }

  auto imGuiFont = ImGui::GetFont();
  auto& imGuiStyle = ImGui::GetStyle();
  auto defaultItemSpacing = imGuiStyle.ItemSpacing;

  // Vertically resize the main window based on console being open/closed
  float outputWindowHeight = canvasSize.y * kOutputWindowWindowScreenHeightPercentage;
  if (!wasConsoleOpen) {
    outputWindowHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
  }

  auto instrument = sequencer.GetInstrument();
  if (instrument != nullptr) {
    ImVec2 sequencerCanvasSize(canvasSize.x, canvasSize.y - outputWindowHeight - mainMenuBarHeight);

    // Main window which contains the tracks on the left and the song lines on the right
    ImGui::SetNextWindowPos(ImVec2(0, mainMenuBarHeight));
    ImGui::SetNextWindowSize(sequencerCanvasSize);
    ImGui::Begin("Sequencer",
      nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
    {
      std::string instrumentName("Instrument");
      char newName[256] = { 0 };
      strcpy(newName, instrument->GetName().c_str());
      if (ImGui::InputText(instrumentName.c_str(), newName, _countof(newName) - 1)) {
        instrument->SetName(std::string(newName));
      }

      ImGui::Separator();

      ImVec2 scrollingCanvasSize(sequencerCanvasSize.x - Globals::kScrollBarWidth,
        sequencerCanvasSize.y - kSequencerWindowToolbarHeight);

      // Vertical scrolling region for tracks and song lines
      ImGui::BeginChild("##ScrollingRegion",
        scrollingCanvasSize,
        false,
        ImGuiWindowFlags_AlwaysAutoResize);
      {
        auto trackLabelWidth = imGuiFont->CalcTextSizeA(imGuiFont->FontSize, FLT_MAX, 0.0f, kTrackNameFormat).x;
        auto trackTotalWidth = trackLabelWidth + kHamburgerMenuWidth + defaultItemSpacing.x;

        // Tracks
        {
          // Leave vertical space for the measure labels in the song line area
          ImGui::NewLine();

          for (uint32 trackIndex = 0; trackIndex < instrument->GetTracks().size(); ++trackIndex) {
            auto& track = instrument->GetTracks()[trackIndex];

            ImVec4 defaultColors[ImGuiCol_COUNT];
            memcpy(defaultColors, imGuiStyle.Colors, sizeof(defaultColors));

            uint32 flashColor = kPlayTrackFlashColor;
            SetTrackColors(track->GetColorScheme(), flashColor);

            // Hamburger menu
            std::string trackHamburgers = std::string("TrackHamburgers") + std::to_string(trackIndex);
            std::string trackProperties = std::string("TrackProperties") + std::to_string(trackIndex);
            ImGui::PushID(trackHamburgers.c_str());
            if (ImGui::Button("=", ImVec2(kHamburgerMenuWidth, kKeyboardKeyHeight))) {
              ImGui::PopID();
              ImGui::OpenPopup(trackProperties.c_str());
            }
            else {
              ImGui::PopID();
            }
            memcpy(imGuiStyle.Colors, defaultColors, sizeof(defaultColors));
            if (ImGui::BeginPopup(trackProperties.c_str())) {
              bool closePopup = false;

              bool mute = track->GetMute();
              if (ImGui::Checkbox("Mute", &mute)) {
                pendingTrackMute = { trackIndex, mute };
              }
              ImGui::SameLine();
              bool solo = instrument->GetSoloTrack() == trackIndex;
              if (ImGui::Checkbox("Solo", &solo)) {
                if (solo) {
                  pendingSoloTrack = trackIndex;
                }
                else {
                  pendingSoloTrack = -1;
                }
              }
              float volume = track->GetVolume();
              if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
                pendingTrackVolume = { trackIndex, volume };
              }
              if (ImGui::Button("Duplicate")) {
                pendingCloneTrack = trackIndex;
                closePopup = true;
              }
              ImGui::SameLine();
              if (ImGui::Button("Delete")) {
                pendingRemoveTrack = trackIndex;
                closePopup = true;
              }
              ImGui::SameLine();
              if (ImGui::Button("Properties...")) {
                pendingDialog = new DialogTrack("Edit Track", trackIndex,
                  new Track(*track), playButtonIconTexture, stopButtonIconTexture);
                closePopup = true;
              }

              ImGui::Spacing();
              ImGui::Spacing();
              ImGui::Spacing();

              closePopup |= ImGui::Button("OK");

              if (closePopup) {
                ImGui::CloseCurrentPopup();
              }
              ImGui::EndPopup();
            }

            imGuiStyle.ItemSpacing.y = 0.0f;
            imGuiStyle.ItemSpacing.x = 0.0f;

            ImGui::SameLine();

            flashColor = kPlayTrackFlashColor;
            SetTrackColors(track->GetColorScheme(), flashColor);

            // Track button
            auto trackButtonBegCursor = ImGui::GetCursorPos();
            if (ImGui::Button(track->GetName().
              c_str(), ImVec2(trackLabelWidth, kKeyboardKeyHeight))) {
              pendingPlayTrack = trackIndex;
            }
            auto trackButtonEndCursor = ImGui::GetCursorPos();

            imGuiStyle.ItemSpacing = defaultItemSpacing;
            memcpy(imGuiStyle.Colors, defaultColors, sizeof(defaultColors));

            // If it's playing, flash it
            float flashPct = 0.0f;
            {
              auto flashTime = playingTrackFlashTimes[0].find(trackIndex);
              if (flashTime != playingTrackFlashTimes[0].end()) {
                auto pct = static_cast<float>((Globals::currentTime - flashTime->second) / kPlayTrackFlashDuration);
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
              ImGui::FillRect(ImVec2(trackLabelWidth, kKeyboardKeyHeight),
                (static_cast<uint32>(flashPct * 255.0f) << 24) | flashColor);
              ImGui::SetCursorPos(trackButtonEndCursor);
            }
          }
        }

        // Song lines
        auto song = sequencer.GetSong();
        if (song != nullptr) {
          // Holy Christ what a mess
          auto parentWindowPos = ImGui::GetWindowPos();
          ImGui::SetNextWindowPos(ImVec2(parentWindowPos.x + trackTotalWidth,
            parentWindowPos.y - ImGui::GetScrollY()));

          ImVec2 songCanvasSize(scrollingCanvasSize.x - trackTotalWidth,
            std::max(ImGui::GetCursorPosY() + Globals::kScrollBarWidth, scrollingCanvasSize.y));

          ImGui::BeginChild("##Song",
            songCanvasSize,
            false,
            ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
          {
            // Measure numbers
            auto measureNumberPos = ImGui::GetCursorPos();
            auto beatLineBegY = 0.0f;
            for (size_t m = 0; m < song->GetNumMeasures(); ++m) {
              ImGui::SetCursorPos(measureNumberPos);
              ImGui::Text(std::to_string(m + 1).c_str());
              beatLineBegY = ImGui::GetCursorPosY();
              measureNumberPos.x += kFullBeatWidth * song->GetBeatsPerMeasure();
            }

            noteSelectedStatus.resize(song->GetLineCount());

            imGuiStyle.ItemSpacing.x = 0.0f;
            imGuiStyle.ItemSpacing.y = 0.0f;

            auto beatWidth = kFullBeatWidth / sequencer.GetSubdivision();
            for (size_t lineIndex = 0; lineIndex < song->GetLineCount(); ++lineIndex) {
              auto& line = song->GetLine(lineIndex);

              ImGui::NewLine();

              // Notes (displayed at current beat zoom level)
              uint32 beatStep = song->GetMinNoteValue() / sequencer.GetSubdivision();
              for (size_t beatIndex = 0; beatIndex < line.size(); beatIndex += beatStep) {
                ImGui::SameLine();

                // Bump the first square by 1 to the right so we can draw a 2-pixel beat line
                if (!beatIndex) {
                  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 1.0f);
                }

                auto uniqueLabel("sl#" + std::to_string(lineIndex) + ":" + std::to_string(beatIndex));

                auto& note = line[beatIndex];

                // Draw empty or filled square radio button depending on whether or not it's enabled
                auto buttonBegPos = ImGui::GetCursorPos();
                auto buttonExtent = ImVec2(beatWidth, kKeyboardKeyHeight);
                if (ImGui::SquareRadioButton(uniqueLabel.c_str(), note.GetEnabled(), buttonExtent.x, buttonExtent.y)) {
                  // Clicking a disabled note enables it; clicking an enabled note selects it
                  if (note.GetEnabled()) {
                    if (!(InputState::Get().modState & KMOD_SHIFT)) {
                      ClearSelectedNotes();
                    }
                    set_toggle<uint32>(noteSelectedStatus[lineIndex], beatIndex);
                  }
                  else {
                    toggledNote = { lineIndex, beatIndex };
                  }
                }

                if (ImGui::IsItemHovered()) {
                  hoveredNote = { lineIndex, beatIndex };
                }

                if (note.GetEnabled()) {
                  // Draw filled note: TODO: Remove this, should draw it once above
                  // https://trello.com/c/Ll8dgleN
                  {
                    ImVec4 noteColor = kDefaultNoteColor;
                    if (note.GetGameIndex() >= 0 && note.GetGameIndex() < _countof(kFretColors)) {
                      noteColor = kFretColors[note.GetGameIndex()];
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
                      set_add<uint32>(noteSelectedStatus[lineIndex], beatIndex);
                    }
                    else {
                      set_remove<uint32>(noteSelectedStatus[lineIndex], beatIndex);
                    }
                  }

                  // Draw selection box around note if selected
                  if (set_contains<uint32>(noteSelectedStatus[lineIndex], beatIndex)) {
                    ImGui::SetCursorPos(ImVec2(buttonBegPos.x + 1.0f, buttonBegPos.y + 1.0f));
                    ImGui::DrawRect(ImVec2(buttonExtent.x - 2.0f,
                      buttonExtent.y - 2.0f), ImGui::ColorConvertFloat4ToU32(kDragSelectColor));
                  }
                }
              }
            }

            ImGui::SetCursorPosY(songCanvasSize.y);
            auto beatLineEndY = ImGui::GetCursorPosY();

            imGuiStyle.ItemSpacing = defaultItemSpacing;

            // Draw the beat demarcation lines
            float cursorPosX = 0.0f;
            for (uint32 m = 0; m < song->GetNumMeasures(); ++m) {
              uint32 c = ImGui::ColorConvertFloat4ToU32(kMeasureDemarcationLineColor);
              for (uint32 b = 0; b < song->GetBeatsPerMeasure(); ++b) {
                ImGui::SetCursorPos(ImVec2(cursorPosX, beatLineBegY));
                ImGui::FillRect(ImVec2(2.0f, beatLineEndY - beatLineBegY), c);
                cursorPosX += kFullBeatWidth;
                c = ImGui::ColorConvertFloat4ToU32(kBeatDemarcationLineColor);
              }
            }

            // Draw the play line
            cursorPosX = beatWidth * (sequencer.GetPosition() /
              (song->GetMinNoteValue() / sequencer.GetSubdivision()));
            ImGui::SetCursorPos(ImVec2(cursorPosX, beatLineBegY));
            ImGui::FillRect(ImVec2(2, beatLineEndY - beatLineBegY),
              ImGui::ColorConvertFloat4ToU32(kPlayLineColor));

            // Drag box
            {
              static ImVec2 mouseDragBeg;

              // On mouse l-press, start the box recording (if we wait for drag to kick in we
              // lose a few pixels)
              if (ImGui::IsMouseClicked(0)) {
                // Don't clear notes if they are attempting to add to the set
                if (!(InputState::Get().modState & KMOD_SHIFT)) {
                  // Don't clear notes if they are beginning a selected group drag
                  if (hoveredNote.first == -1 || !set_contains<uint32>(noteSelectedStatus[hoveredNote.first], hoveredNote.second)) {
                    ClearSelectedNotes();
                  }
                }

                mouseDragBeg = ImGui::GetMousePos();
                mouseDragBeg -= ImGui::GetWindowPos();
              }

              // On mouse release, clear drag box
              if (ImGui::IsMouseReleased(0)) {
                dragBox = { -1.0f, -1.0f, -1.0f, -1.0f };
              }

              // Update drag box while dragging
              if (ImGui::IsMouseDragging()) {
                auto mouseDragCur = ImGui::GetIO().MousePos;

                if (mouseDragCur.x != -FLT_MAX && mouseDragCur.y != -FLT_MAX) {
                  mouseDragCur -= ImGui::GetWindowPos();

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
                ImGui::DrawRect(ImVec2(static_cast<float>(w),
                  static_cast<float>(h)), ImGui::GetColorU32(kDragBoxColor));
                ImGui::SetCursorPos(oldCursorPos);
              }
            }

            ImGui::EndChild();
          } // Song child region          
        } // if (song != nullptr)

        ImGui::EndChild();
      } // Track + Song vertical scroling region

      // Bottom tool bar
      {
        auto song = sequencer.GetSong();

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

        // BPM
        ImGui::SameLine();
        ImGui::PushItemWidth(100);
        int currentBpm = song ? song->GetTempo() : Globals::kDefaultTempo;
        if (ImGui::InputInt("BPM", &currentBpm)) {
          // @Delay
          pendingTempo = currentBpm;
        }
        ImGui::PopItemWidth();

        // Grid (subdivision)
        ImGui::SameLine();
        ImGui::PushItemWidth(100);
        if (ImGui::BeginCombo("Subdivision", (std::string("1/") +
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
              // @Delay
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
        bool isLooping = sequencer.IsLooping();
        if (ImGui::Checkbox("Loop", &isLooping)) {
          // @Atomic
          sequencer.SetLooping(isLooping);
        }
        ImGui::PopItemWidth();

        // Metronome
        ImGui::SameLine();
        ImGui::PushItemWidth(100);
        bool isMetronomeOn = sequencer.IsMetronomeOn();
        if (ImGui::Checkbox("Metronome", &isMetronomeOn)) {
          // @Atomic
          sequencer.EnableMetronome(isMetronomeOn);
        }
        ImGui::PopItemWidth();

        // Master volume
        ImGui::SameLine();
        ImGui::PushItemWidth(100);
        float masterVolume = Mixer::Get().GetMasterVolume();
        if (ImGui::SliderFloat("Master", &masterVolume, 0.0f, 1.0f)) {
          // @Atomic
          Mixer::Get().SetMasterVolume(masterVolume);
        }
      } // Bottom toolbar

      ImGui::End();
    } // Sequencer window
  } // if (instrument != nullptr)

  // Output window
  int32 outputWindowTop = static_cast<int32>(canvasSize.y - outputWindowHeight);
  ImGui::SetNextWindowPos(ImVec2(0, static_cast<float>(outputWindowTop)));
  ImGui::SetNextWindowSize(ImVec2(canvasSize.x, static_cast<float>(outputWindowHeight)));
  wasConsoleOpen = ImGui::Begin("Output",
    nullptr,
    ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
  {
    // Options menu
    if (ImGui::BeginPopup("Options"))
    {
      if (ImGui::MenuItem("Clear")) {
        outputWindowState.ClearLog();
      }
      if (ImGui::MenuItem("Scroll to bottom")) {
        outputWindowState.scrollToBottom = true;
      }
      bool autoScroll = outputWindowState.autoScroll;
      if (ImGui::Checkbox("Auto-scroll", &autoScroll)) {
        outputWindowState.autoScroll = autoScroll;
        if (autoScroll) {
          outputWindowState.scrollToBottom = true;
        }
      }
      ImGui::EndPopup();
    }

    if (ImGui::SmallButton("=")) {
      ImGui::OpenPopup("Options");
    }
    ImGui::SameLine();
    ImGui::Text("Voices: %d", Mixer::Get().GetNumActiveVoices());
    ImGui::SameLine();
    ImGui::Text("Mode: %s", kModeStrings[static_cast<uint32>(mode)]);
    ImGui::Separator();

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    {
      // OutputWindow stuff
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing

      for (const auto& historyText : outputWindowState.displayHistory) {
        if (historyText.length() < 1) {
          continue;
        }

        // TODO: rather than parse the string, create an expanded class
        // https://trello.com/c/wksKPcRD
        static const ImVec4 logColors[Logging::Category::Count] = {
          ImVec4(1.0f, 1.0f, 1.0f, 1.0f), // Info = White
          ImVec4(0.7f, 0.7f, 0.0f, 1.0f), // Warn = Yellow
          ImVec4(0.7f, 0.0f, 0.0f, 1.0f), // Error = Red
          ImVec4(1.0f, 0.0f, 0.0f, 1.0f), // Fatal = Bright Red
        };

        auto logCategory = static_cast<Logging::Category>(*historyText.c_str() - 1);
        ImGui::PushStyleColor(ImGuiCol_Text, logColors[logCategory]);
        ImGui::TextUnformatted(historyText.c_str() + 1);
        ImGui::PopStyleColor();
      }
    }

    if (outputWindowState.scrollToBottom) {
      ImGui::SetScrollY(ImGui::GetScrollMaxY());
      ImGui::SetScrollHere(1.0f);
      outputWindowState.scrollToBottom = false;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
  }

  ImGui::End();

  renderable.Render();
}

void ComposerView::NotePlayedCallback(uint32 trackIndex, uint32 noteIndex) {
  playingTrackFlashTimes[1].insert({ trackIndex, Globals::currentTime });
}

void ComposerView::InitResources() {
  // Backup GL state
  GLint lastTexture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);

  // Load UI textures
  int width, height;
  glGenTextures(1, &playButtonIconTexture);
  glBindTexture(GL_TEXTURE_2D, playButtonIconTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  uint8* iconData = SOIL_load_image("Assets\\play_icon.png", &width, &height, 0, SOIL_LOAD_RGBA);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconData);
  SOIL_free_image_data(iconData);

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

ComposerView::ComposerView(uint32 mainWindowHandle)
: mainWindowHandle(mainWindowHandle) {

  logResponderId = Logging::AddResponder([=](const std::string_view& logLine) {
    outputWindowState.AddLog(logLine);
  });

  InitResources();
}

void ComposerView::Show() {
  // Songs refer to instruments; if you load a song and it does not match the current
  // instrument it needs a way to 'talk back' to us and ask us to load the correct
  // instrument.
  auto _mainWindowHandle = mainWindowHandle;
  Sequencer::Get().SetLoadInstrumentCallback(
    [_mainWindowHandle](std::string instrumentName) {
      return LoadInstrument(reinterpret_cast<HWND>(_mainWindowHandle), instrumentName);
    });

  notePlayedCallbackId = Sequencer::Get().AddNotePlayedCallback(
    [](uint32 trackIndex, uint32 noteIndex, void* payload) {
      reinterpret_cast<ComposerView*>(payload)->NotePlayedCallback(trackIndex, noteIndex);
    }, this);
}

void ComposerView::Hide() {
  if (notePlayedCallbackId != UINT32_MAX) {
    Sequencer::Get().RemoveNotePlayedCallback(notePlayedCallbackId);
    notePlayedCallbackId = UINT32_MAX;
  }
}

ComposerView::~ComposerView() {
  if (logResponderId != UINT32_MAX) {
    Logging::PopResponder(logResponderId);
    logResponderId = UINT32_MAX;
  }
  delete activeDialog;
  activeDialog = nullptr;
  delete pendingDialog;
  pendingDialog = nullptr;
}