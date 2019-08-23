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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
#include <commdlg.h>
#include <atlbase.h>
#include <commctrl.h>

static constexpr float kFullBeatWidth = 80.0f;
static constexpr float kKeyboardKeyWidth = 100.0f;
static constexpr float kKeyboardKeyHeight = 20.0f;
static constexpr uint32 kDefaultBpm = 120;
static constexpr uint32 kDefaultNumMeasures = 2;
static constexpr uint32 kDefaultBeatsPerMeasure = 4;
static constexpr uint32 kDefaultSubdivisions = 4;
static constexpr float kPlayNoteFlashDuration = 0.5f;
static constexpr float kPlayNoteFlashGrow = 1.0f;
static constexpr uint32 kPlayNoteFlashColor = 0x0000FFFF;
static constexpr uint32 kMaxMeasures = 256;
static constexpr uint32 kMinMeasures = 1;
static constexpr uint32 kMaxBeatsPerMeasure = 12;
static constexpr uint32 kMinBeatsPerMeasure = 2;
static constexpr float kDefaultNoteVelocity = 1.0f;
static constexpr uint32 kPlayTrackFlashColor = 0x00007F7F;
static constexpr float kPlayTrackFlashDuration = 0.5f;
static constexpr float kOutputWindowWindowScreenHeightPercentage = 0.35f;
static constexpr float kSequencerWindowToolbarHeight = 64.0f;
static constexpr float kHamburgerMenuWidth(20.0f);
static constexpr std::string_view kJsonTag(".json");
static constexpr const char* kDefaultNewTrackName("NewTrack");
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

void ComposerView::GroupOrHoveredAction(std::function<void(int32, int32)> action) {
  auto instrument = Sequencer::Get().GetInstrument();
  if (instrument != nullptr) {
    bool anySelected = false;
    for (size_t trackIndex = 0; trackIndex < noteSelectedStatus.size(); ++trackIndex) {
      for (size_t noteIndex = 0; noteIndex < noteSelectedStatus[trackIndex].size(); ++noteIndex) {
        if (noteSelectedStatus[trackIndex][noteIndex]) {
          anySelected = true;
          action(trackIndex, noteIndex);
        }
      }
    }

    if (!anySelected && hoveredNote.second != -1) {
      action(hoveredNote.first, hoveredNote.second);
    }
  }
}

void ComposerView::HandleInput() {
  auto& inputState = InputState::Get();

  // Saving tired right index fingers since 2018
  if (inputState.pressed[SDLK_SPACE]) {
    GroupOrHoveredAction([](int32 trackIndex, int32 noteIndex) {
      auto track = Sequencer::Get().GetInstrument()->GetTrack(trackIndex);
      if (track != nullptr) {
        auto& note = track->GetNote(noteIndex);
        note.enabled = !note.enabled;
      }
    });

    ClearSelectedNotes();
  }

  if (inputState.pressed[SDLK_ESCAPE]) {
    ClearSelectedNotes();
  }

  if (inputState.modState & KMOD_CTRL) {
    // Copy
    if (inputState.pressed[SDLK_c]) {
      if (noteSelectedStatus.size() > 0) {
        noteClipboard = noteSelectedStatus;
      }
    }

    // Paste
    else if (inputState.pressed[SDLK_v]) {
      if (noteClipboard.size()) {
        auto instrument = Sequencer::Get().GetInstrument();
        if (instrument != nullptr) {
          // We need to know the first note to generate the proper offset
          size_t firstNote = INT32_MAX;
          for (size_t t = 0; t < noteClipboard.size(); ++t) {
            for (size_t n = 0; n < noteClipboard[t].size() && n < firstNote; ++n) {
              if (noteClipboard[t][n] != 0) {
                firstNote = n;
                break;
              }
            }
          }

          if (firstNote != INT32_MAX) {
            for (size_t t = 0; t < noteClipboard.size(); ++t) {
              auto track = instrument->GetTrack(t);

              size_t e = track->GetNoteCount();
              for (size_t n = 0; n < noteClipboard[t].size(); ++n) {
                if (noteClipboard[t][n] != 0) {
                  track->SetNote(e + n - firstNote, track->GetNote(n));
                }
              }
            }
          }
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
        int32 newFretIndex = -2;

        if (inputState.pressed[SDLK_1]) {
          newFretIndex = 0;
        }
        else if (inputState.pressed[SDLK_2]) {
          newFretIndex = 1;
        }
        else if (inputState.pressed[SDLK_3]) {
          newFretIndex = 2;
        }
        else if (inputState.pressed[SDLK_4]) {
          newFretIndex = 3;
        }
        else if (inputState.pressed[SDLK_0]) {
          newFretIndex = -1;
        }

        if (newFretIndex != -2) {
          GroupOrHoveredAction([newFretIndex](int32 trackIndex, int32 noteIndex) {
            auto track = Sequencer::Get().GetInstrument()->GetTrack(trackIndex);
            if (track != nullptr) {
              track->GetNote(noteIndex).fretIndex = newFretIndex;
            }
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
  playingNotesFlashTimes[0].merge(playingNotesFlashTimes[1]);

  // Number of measures changed
  if (pendingNumMeasures != -1) {
    sequencer.SetNumMeasures(pendingNumMeasures);
  }

  // Beats per measure changed
  if (pendingBeatsPerMeasure != -1) {
    sequencer.SetBeatsPerMeasure(pendingBeatsPerMeasure);
  }

  // Subdivision changed
  if (pendingSubdivision != -1) {
    sequencer.SetSubdivision(pendingSubdivision);
  }

  // Beats per minute changed
  if (pendingBeatsPerMinute != -1) {
    sequencer.SetBeatsPerMinute(pendingBeatsPerMinute);
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
      auto track = instrument->GetTrack(toggledNote.first);
      if (track != nullptr) {
        auto note = track->GetNote(toggledNote.second);
        if (note.enabled) {
          note.enabled = false;
        }
        else {
          note.enabled = true;
        }
        track->SetNote(toggledNote.second, note);
      }
    }

    // Change a note's fret index

    // Track cloned via dialog previous frame
    if (pendingCloneTrack != -1) {
      auto oldTrack = instrument->GetTrack(pendingCloneTrack);
      if (oldTrack != nullptr) {
        auto newTrack = new Track(*oldTrack);
        newTrack->SetName(GetNewTrackName(oldTrack->GetName()));
        instrument->AddTrack(newTrack);
      }
    }

    // Track removed via dialog previous frame
    // NOTE: This is the last delayed track operation just in case
    if (pendingRemoveTrack != -1) {
      noteSelectedStatus.erase(noteSelectedStatus.begin() + pendingRemoveTrack);
      instrument->RemoveTrack(pendingRemoveTrack);
    }
  }

  // Reset all pendings
  pendingNumMeasures = -1;
  pendingBeatsPerMeasure = -1;
  pendingSubdivision = -1;
  pendingBeatsPerMinute = -1;
  pendingMasterVolume = -1.0f;
  pendingTrackVolume = { -1, 0.0f };
  pendingTrackMute = { -1, false };
  pendingPlayTrack = -1;
  pendingRemoveTrack = -1;
  pendingCloneTrack = -1;
  pendingSoloTrack = -2;
  toggledNote = { -1, -1 };
}

void ComposerView::ClearSelectedNotes() {
  for (auto& nss : noteSelectedStatus) {
    std::fill(nss.begin(), nss.end(), false);
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
        ClearSelectedNotes();
        sequencer.NewInstrument();
      }
      if (ImGui::MenuItem("Load Instrument")) {
        ClearSelectedNotes();
        LoadInstrument(reinterpret_cast<HWND>(mainWindowHandle));
      }
      if (ImGui::MenuItem("Save Instrument")) {
        SaveInstrument();
      }
      if (ImGui::MenuItem("Exit")) {
        SDL_PushEvent(&SDL_Event({ SDL_QUIT }));
      }
      ImGui::EndMenu();
    }
    if (sequencer.GetInstrument() != nullptr) {
      if (ImGui::BeginMenu("Instrument")) {
        if (ImGui::MenuItem("Add Track")) {
          pendingDialog = new DialogTrack("Add Track", sequencer.GetInstrument(),
            -1, new Track(GetNewTrackName(kDefaultNewTrackName)), playButtonIconTexture, stopButtonIconTexture);
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Song")) {
        if (ImGui::MenuItem("New Song")) {
          ClearSelectedNotes();
          sequencer.GetInstrument()->ClearNotes();
        }
        if (ImGui::MenuItem("Load Song")) {
          ClearSelectedNotes();
          LoadSong();
        }
        if (ImGui::MenuItem("Save Song")) {
          SaveSong();
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

  // Resize the main window based on console being open/closed
  int32 outputWindowHeight = static_cast<int32>(canvasSize.y * kOutputWindowWindowScreenHeightPercentage);
  if (!wasConsoleOpen) {
    outputWindowHeight = 20;
  }
  auto const sequencerHeight = static_cast<int32>(canvasSize.y - outputWindowHeight - mainMenuBarHeight);

  auto instrument = sequencer.GetInstrument();

  if (instrument != nullptr) {
    // Ensure selected note tracker is proper size
    uint32 beatCount = Sequencer::Get().GetNumMeasures() *
      Sequencer::Get().GetBeatsPerMeasure() * Sequencer::Get().GetMaxSubdivisions();
    noteSelectedStatus.resize(instrument->GetTrackCount());
    for (auto& nss : noteSelectedStatus) {
      nss.resize(beatCount);
    }

    ImGui::SetNextWindowPos(ImVec2(0, mainMenuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(canvasSize.x), static_cast<float>(sequencerHeight)));
    ImGui::Begin("Instrument",
      nullptr,
      ImGuiWindowFlags_NoTitleBar | 
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
    {
      auto& imGuiStyle = ImGui::GetStyle();
      auto oldItemSpacing = imGuiStyle.ItemSpacing;

      std::string instrumentName("Instrument");
      char newName[256] = { 0 };
      strcpy(newName, instrument->GetName().c_str());
      if (ImGui::InputText(instrumentName.c_str(), newName, _countof(newName) - 1)) {
        instrument->SetName(std::string(newName));
      }

      ImGui::NewLine();
      ImGui::Separator();

      auto beatWidth = kFullBeatWidth / sequencer.GetSubdivision();

      // Start of the beat label
      float beatLabelStartX = ImGui::GetCursorPosX() + kHamburgerMenuWidth + kKeyboardKeyWidth + imGuiStyle.ItemSpacing.x;

      // Measure numbers
      float cursorPosX = beatLabelStartX;
      for (size_t m = 0; m < sequencer.GetNumMeasures(); ++m) {
        ImGui::SetCursorPosX(cursorPosX);
        cursorPosX += kFullBeatWidth * sequencer.GetBeatsPerMeasure();
        ImGui::Text(std::to_string(m + 1).c_str());
        ImGui::SameLine();
      }

      ImGui::NewLine();
      ImGui::Separator();

      auto beatLabelStartY = ImGui::GetCursorPosY();

      // Tracks
      ImGui::BeginChild("##InstrumentScrollingRegion",
        ImVec2(static_cast<float>(canvasSize.x) - Globals::kScrollBarWidth,
        static_cast<float>(sequencerHeight) - kSequencerWindowToolbarHeight - beatLabelStartY),
        false,
        ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
      {
        uint32 flashColor;
        uint32 noteGlobalIndex = 0;
        for (uint32 trackIndex = 0; trackIndex < instrument->GetTracks().size(); ++trackIndex) {
          auto& track = instrument->GetTracks()[trackIndex];

          ImVec4 oldColors[ImGuiCol_COUNT];
          memcpy(oldColors, imGuiStyle.Colors, sizeof(oldColors));

          imGuiStyle.ItemSpacing = oldItemSpacing;

          flashColor = kPlayTrackFlashColor;
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
          memcpy(imGuiStyle.Colors, oldColors, sizeof(oldColors));
          if (ImGui::BeginPopup(trackProperties.c_str())) {
            bool closePopup = false;

            bool mute = track->GetMute();
            if (ImGui::Checkbox("Mute", &mute)) {
              // @Delay
              pendingTrackMute = { trackIndex, mute };
            }
            ImGui::SameLine();
            bool solo = instrument->GetSoloTrack() == trackIndex;
            if (ImGui::Checkbox("Solo", &solo)) {
              // @Delay
              if (solo) {
                pendingSoloTrack = trackIndex;
              }
              else {
                pendingSoloTrack = -1;
              }
            }
            float volume = track->GetVolume();
            if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
              // @Delay
              pendingTrackVolume = { trackIndex, volume };
            }
            if (ImGui::Button("Duplicate")) {
              // @Delay
              pendingCloneTrack = trackIndex;
              closePopup = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
              // @Delay
              pendingRemoveTrack = trackIndex;
              closePopup = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Properties...")) {
              pendingDialog = new DialogTrack("Edit Track", instrument, trackIndex,
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

          imGuiStyle.ItemSpacing.x = 0.0f;
          ImGui::SameLine();

          auto prevPos = ImGui::GetCursorPos();

          flashColor = kPlayTrackFlashColor;
          SetTrackColors(track->GetColorScheme(), flashColor);

          // Track key
          if (ImGui::Button(track->GetName().
            c_str(), ImVec2(kKeyboardKeyWidth, kKeyboardKeyHeight))) {
            // @Delay
            pendingPlayTrack = trackIndex;
          }

          imGuiStyle.ItemSpacing.x = oldItemSpacing.x;

          auto currPos = ImGui::GetCursorPos();

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
            prevPos.x -= kPlayNoteFlashGrow * flashPct;
            prevPos.y -= kPlayNoteFlashGrow * flashPct;

            ImGui::SetCursorPos(prevPos);
            ImGui::FillRect(ImVec2(kKeyboardKeyWidth + kPlayNoteFlashGrow * 2.0f * flashPct,
              kKeyboardKeyHeight + kPlayNoteFlashGrow * 2.0f * flashPct),
              (static_cast<uint32>(flashPct * 255.0f) << 24) | flashColor);
            ImGui::SetCursorPos(currPos);
          }

          memcpy(imGuiStyle.Colors, oldColors, sizeof(oldColors));

          // Beat groups
          uint32 noteLocalIndex = 0;
          for (size_t b = 0; b < sequencer.GetNumMeasures() * sequencer.GetBeatsPerMeasure(); ++b) {
            // Notes
            for (size_t s = 0; s < sequencer.GetSubdivision(); ++s) {
              ImGui::SameLine();

              // Lesson learned: labels are required to pair UI with UX
              auto uniqueLabel(track->GetName() + std::string("#") + std::to_string(noteLocalIndex));

              auto& trackNote = track->GetNote(noteLocalIndex);

              // Toggle notes that are clicked
              imGuiStyle.ItemSpacing.x = 0.0f;
              imGuiStyle.ItemSpacing.y = 0.0f;

              auto cursorPos = ImGui::GetCursorPos();
              if (ImGui::SquareRadioButton(uniqueLabel.c_str(), trackNote.enabled, beatWidth, kKeyboardKeyHeight)) {
                if (InputState::Get().modState & KMOD_SHIFT) {
                  if (trackNote.enabled) {
                    noteSelectedStatus[trackIndex][noteLocalIndex] = !noteSelectedStatus[trackIndex][noteLocalIndex];
                  }
                }
                else {
                  ClearSelectedNotes();

                  // @Delay
                  toggledNote = { trackIndex, noteLocalIndex };
                }
              }

              if (ImGui::IsItemHovered()) {
                hoveredNote = { trackIndex, noteLocalIndex };
              }

              // Draw filled note
              if (trackNote.enabled) {
                ImVec4 noteColor = kDefaultNoteColor;
                if (trackNote.fretIndex >= 0 && trackNote.fretIndex < _countof(kFretColors)) {
                  noteColor = kFretColors[trackNote.fretIndex];
                }
                auto currentPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(cursorPos);
                ImGui::FillRect(ImVec2(beatWidth,
                  kKeyboardKeyHeight), ImGui::ColorConvertFloat4ToU32(noteColor));

                // If we're drag-selecting, update the selected status
                if (dragBox.x >= 0.0f) {
                  glm::vec4 noteBox(cursorPos.x, cursorPos.y,
                    cursorPos.x + beatWidth, cursorPos.y + kKeyboardKeyHeight);

                  if (dragBox.x <= noteBox.z && noteBox.x <= dragBox.z &&
                      dragBox.w >= noteBox.y && noteBox.w >= dragBox.y) {
                    noteSelectedStatus[trackIndex][noteLocalIndex] = true;
                  }
                  else {
                    noteSelectedStatus[trackIndex][noteLocalIndex] = false;
                  }
                }

                // Draw selection box around note if selected
                if (noteSelectedStatus[trackIndex][noteLocalIndex]) {
                  auto currentPos = ImGui::GetCursorPos();
                  ImGui::SetCursorPos(cursorPos);
                  ImGui::DrawRect(ImVec2(beatWidth,
                    kKeyboardKeyHeight), ImGui::ColorConvertFloat4ToU32(kDragSelectColor));
                }

                // If it's playing, flash it
                float flashPct = 0.0f;
                {
                  auto flashTime = playingNotesFlashTimes[0].find(noteGlobalIndex);
                  if (flashTime != playingNotesFlashTimes[0].end()) {
                    auto pct = static_cast<float>((Globals::currentTime - flashTime->second) / kPlayNoteFlashDuration);
                    if (pct >= 1.0f) {
                      playingNotesFlashTimes[0].erase(flashTime);
                    }
                    else {
                      flashPct = 1.0f - pct;
                    }
                  }
                }

                if (flashPct > 0.0f) {
                  auto flashCursorPos = cursorPos;

                  flashCursorPos.x -= kPlayNoteFlashGrow * flashPct;
                  flashCursorPos.y -= kPlayNoteFlashGrow * flashPct;

                  ImGui::SetCursorPos(flashCursorPos);
                  ImGui::FillRect(ImVec2(beatWidth + kPlayNoteFlashGrow * 2.0f * flashPct,
                    kKeyboardKeyHeight + kPlayNoteFlashGrow * 2.0f * flashPct),
                    (static_cast<uint32>(flashPct * 255.0f) << 24) | kPlayNoteFlashColor);
                }

                ImGui::SetCursorPos(currentPos);
              }

              noteLocalIndex += sequencer.GetMaxSubdivisions() / sequencer.GetSubdivision();
              noteGlobalIndex += sequencer.GetMaxSubdivisions() / sequencer.GetSubdivision();
            }
          }

          // Reset old X spacing to offset from keyboard key
          imGuiStyle.ItemSpacing.x = oldItemSpacing.x;
        }
      }
      // Drag box
      {
        static ImVec2 mouseDragBeg;

        // On mouse l-press, start the box recording (if we wait for drag to kick in we
        // lose a few pixels)
        if (ImGui::IsMouseClicked(0)) {
          ClearSelectedNotes();

          mouseDragBeg = ImGui::GetMousePos();
          if (mouseDragBeg.x < beatLabelStartX) {
            mouseDragBeg.x = beatLabelStartX;
          }
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
            if (mouseDragCur.x < beatLabelStartX) {
              mouseDragCur.x = beatLabelStartX;
            }

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

      imGuiStyle.ItemSpacing = oldItemSpacing;

      float beatLabelEndY = ImGui::GetCursorPosY() - imGuiStyle.ItemSpacing.y;

      ImGui::Separator();
      ImGui::NewLine();

      // Bottom tool bar
      {
        imGuiStyle.ItemSpacing.x = 2;

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

        imGuiStyle.ItemSpacing.x = oldItemSpacing.x + 5;

        // Measures
        ImGui::SameLine();
        ImGui::PushItemWidth(100);
        int numMeasures = sequencer.GetNumMeasures();
        if (ImGui::InputInt("Measures", &numMeasures)) {
          // @Delay
          pendingNumMeasures = std::max(std::min(static_cast<uint32>(numMeasures), kMaxMeasures), kMinMeasures);
        }
        ImGui::PopItemWidth();

        // Beats per measure
        ImGui::SameLine();
        ImGui::PushItemWidth(75);
        int beatsPerMeasure = sequencer.GetBeatsPerMeasure();
        if (ImGui::InputInt("/", &beatsPerMeasure)) {
          // @Delay
          pendingBeatsPerMeasure = std::max(std::min(static_cast<uint32>
            (beatsPerMeasure), kMaxBeatsPerMeasure), kMinBeatsPerMeasure);
        }
        ImGui::PopItemWidth();

        imGuiStyle.ItemSpacing.x = 3;

        // Subdivision
        ImGui::SameLine();
        ImGui::PushItemWidth(50);
        if (ImGui::BeginCombo("#", std::to_string(sequencer.GetSubdivision()).c_str())) {
          std::vector<uint32> subDivs;

          uint32 subDiv = Sequencer::Get().GetMaxSubdivisions();
          while (subDiv >= 2) {
            subDivs.push_back(subDiv);
            subDiv /= 2;
          }

          for (auto revIter = subDivs.rbegin(); revIter != subDivs.rend(); ++revIter) {
            bool isSelected = (sequencer.GetSubdivision() == *revIter);
            if (ImGui::Selectable(std::to_string(*revIter).c_str(), isSelected)) {
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

        imGuiStyle.ItemSpacing.x = oldItemSpacing.x + 5;

        // BPM
        ImGui::SameLine();
        ImGui::PushItemWidth(100);
        int currentBpm = sequencer.GetBeatsPerMinute();
        if (ImGui::InputInt("BPM", &currentBpm)) {
          // @Delay
          pendingBeatsPerMinute = currentBpm;
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
      }

      auto oldCursorPos = ImGui::GetCursorPos();

      // Draw the beat demarcation lines
      cursorPosX = beatLabelStartX;
      for (uint32 m = 0; m < sequencer.GetNumMeasures(); ++m) {
        float a = 1.0f;
        float w = 3.0f;
        for (uint32 b = 0; b < sequencer.GetBeatsPerMeasure(); ++b) {
          ImGui::SetCursorPos(ImVec2(cursorPosX - a, beatLabelStartY));
          ImGui::FillRect(ImVec2(w, beatLabelEndY -
            beatLabelStartY), ImGui::GetColorU32(ImGuiCol_FrameBgActive));
          cursorPosX += kFullBeatWidth;
          a = 0;
          w = 1;
        }
      }
      imGuiStyle.ItemSpacing = oldItemSpacing;

      // Draw the play line
      cursorPosX = beatLabelStartX + beatWidth * (sequencer.GetTrackPosition() /
        (sequencer.GetMaxSubdivisions() / sequencer.GetSubdivision()));
      ImGui::SetCursorPos(ImVec2(cursorPosX - 0, beatLabelStartY));
      ImGui::FillRect(ImVec2(1, beatLabelEndY - beatLabelStartY), 0x7FFFFFFF);

      ImGui::SetCursorPos(oldCursorPos);
    }

    ImGui::End();
  }

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
  playingNotesFlashTimes[1].insert({ Sequencer::Get().GetInstrument()->
    GetTrack(trackIndex)->GetNoteCount() * trackIndex + noteIndex, Globals::currentTime });
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
  // TODO: Gotta rethink this
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