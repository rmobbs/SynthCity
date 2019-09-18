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
#include "SerializeImpl.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
#include <commdlg.h>
#include <atlbase.h>
#include <commctrl.h>
#include <fstream>

static constexpr float kFullBeatWidth = 80.0f;
static constexpr float kKeyboardKeyHeight = 20.0f;
static constexpr uint32 kPlayTrackFlashColor = 0x00007F7F;
static constexpr float kPlayTrackFlashDuration = 0.5f;
static constexpr float kOutputWindowWindowScreenHeightPercentage = 0.35f;
static constexpr float kSequencerWindowToolbarHeight = 74.0f;
static constexpr float kHamburgerMenuWidth(20.0f);
static constexpr const char* kDefaultNewTrackName("NewTrack");
static constexpr const char* kTrackNameFormat("XXXXXXXXXXXXXXXX");
static constexpr const char* kModeStrings[] = {
  "Normal",
  "Markup",
};

static const ImVec4 kDefaultNoteColor(1.0f, 1.0f, 1.0f, 0.5f);
static const ImVec4 kDragBoxOutlineColor(0.0f, 0.0f, 0.0f, 1.0f);
static const ImVec4 kDragBoxFillColor(0.9f, 0.9f, 0.9f, 0.5f);
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
static constexpr uint32 kMinMeasuresToAdd = 1;
static constexpr uint32 kMaxMeasuresToAdd = 256;

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

std::string ComposerView:: GetNewInstrumentName(std::string instrumentNameBase) {
  // TODO: When we allow multiple instruments, this will need to loop through them!
  return instrumentNameBase;
}

void ComposerView::NewInstrument() {
  Sequencer::Get().GetSong()->SetInstrument(new
    Instrument(GetNewInstrumentName(Instrument::kDefaultName)));
}

Instrument* ComposerView::LoadInstrument(std::string requiredInstrument) {
  WCHAR szFile[FILENAME_MAX] = { 0 };
  OPENFILENAME ofn = { 0 };

  USES_CONVERSION;
  ofn.lStructSize = sizeof(ofn);

  std::string windowTitle("Load Instrument");
  if (!requiredInstrument.empty()) {
    windowTitle += " " + requiredInstrument;
  }
  ofn.lpstrTitle = A2W(windowTitle.c_str());
  ofn.hwndOwner = reinterpret_cast<HWND>(mainWindowHandle);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
  ofn.lpstrFilter = _TEXT("JSON\0*.json\0");
  ofn.nFilterIndex = 0;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  Instrument* instrument = nullptr;

  while (instrument == nullptr) {
    if (GetOpenFileName(&ofn)) {
      instrument = Instrument::LoadInstrument(std::string(W2A(szFile)));
      if (instrument) {
        if (!requiredInstrument.empty() && instrument->GetName() != requiredInstrument) {
          delete instrument;
          instrument = nullptr;

          // Hit cancel on error message
          if (IDCANCEL == MessageBox(reinterpret_cast<HWND>(mainWindowHandle),
            _TEXT("Song requires a different instrument"),
            _TEXT("Error"), MB_OKCANCEL)) {
            break;
          }
          // Try again
        }
        // Correct instrument successfully loaded
      }
      // Instrument failed to load
      else {
        // Hit cancel on error message
        if (IDCANCEL == MessageBox(reinterpret_cast<HWND>(mainWindowHandle),
          _TEXT("Selected instrument failed to load"),
          _TEXT("Error"), MB_OKCANCEL)) {
          break;
        }
        // Try again
      }
    }
    // Hit cancel on load file dialog
    else {
      break;
    }
  }

  return instrument;
}

void ComposerView::SaveInstrument() {
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
    Sequencer::Get().GetSong()->GetInstrument()->SaveInstrument(W2A(szFile));
  }
}

void ComposerView::NewSong() {
  auto song = new Song(Song::kDefaultName, Globals::kDefaultTempo,
    0, 0, Song::kDefaultBeatsPerMeasure, Globals::kDefaultMinNote);

  Sequencer::Get().SetSong(song);
}

void ComposerView::LoadSong() {
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
    auto song = Song::LoadSong(W2A(szFile), [=](std::string requiredInstrumentName) {
      return LoadInstrument(requiredInstrumentName);
    });

    if (song != nullptr) {
      Sequencer::Get().SetSong(song);
    }
  }
}

void ComposerView::SaveSong() {
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
    Sequencer::Get().GetSong()->Save(W2A(szFile));
  }
}

std::string ComposerView::GetNewTrackName(std::string trackNameBase) {
  // Pick an available name
  std::string trackName = trackNameBase;
  // Just feels weird and shameful to not have an upper bounds ...
  for (int nameSuffix = 1; nameSuffix < 1000; ++nameSuffix) {
    auto& tracks = Sequencer::Get().GetSong()->GetInstrument()->GetTracks();

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
      Sequencer::Get().GetSong()->RemoveNote(lineIndex, noteIndex);
    });

    ClearSelectedNotes();
    
    // Sadly, might have deleted current head
    OnSongUpdated();
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
    SelectedGroupAction([newGameIndex](int32 lineIndex, int32 beatIndex) {
      auto& line = Sequencer::Get().GetSong()->GetLine(lineIndex);
      for (auto noteIter = line.begin(); noteIter != line.end(); ++noteIter) {
        if (noteIter->GetBeatIndex() == beatIndex) {
          noteIter->SetGameIndex(newGameIndex);
          break;
        }
      }
    });
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

  auto song = sequencer.GetSong();
  if (song != nullptr) {
    auto instrument = song->GetInstrument();
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

    // Toggle a note on
    if (toggledNote.second != -1) {
      song->AddNote(toggledNote.first, toggledNote.second);
    }

    if (pendingAddMeasures != -1) {
      song->AddMeasures(pendingAddMeasures);
    }

    // Beats per minute changed
    if (pendingTempo != -1) {
      song->SetTempo(pendingTempo);
    }
  }

  if (pendingNewInstrument) {
    ClearSelectedNotes();
    NewInstrument();
  }

  if (pendingLoadInstrument) {
    ClearSelectedNotes();
    auto instrument = LoadInstrument({});
    if (instrument != nullptr) {
      song->SetInstrument(instrument);
    }
  }

  if (pendingSaveInstrument) {
    SaveInstrument();
  }

  if (pendingNewSong) {
    ClearSelectedNotes();
    NewSong();
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
  pendingTrackVolume = { -1, 0.0f };
  pendingTrackMute = { -1, false };
  pendingPlayTrack = -1;
  pendingRemoveTrack = -1;
  pendingCloneTrack = -1;
  pendingSoloTrack = -2;
  pendingAddMeasures = -1;
  pendingNewInstrument = false;
  pendingLoadInstrument = false;
  pendingSaveInstrument = false;
  pendingNewSong = false;
  pendingLoadSong = false;
  pendingSaveSong = false;
  toggledNote = { -1, -1 };
}

void ComposerView::ClearSelectedNotes() {
  for (auto& nss : noteSelectedStatus) {
    nss.clear();
  }
}

void ComposerView::OnSongUpdated() {
  songIterator.clear();

  auto song = Sequencer::Get().GetSong();
  if (song != nullptr) {
    songIterator = song->GetIterator();
  }
}

void ComposerView::OnBeat(uint32 beatIndex) {
  auto& sequencer = Sequencer::Get();
  auto song = sequencer.GetSong();

  if (isMetronomeOn) {
    sequencer.PlayMetronome((beatIndex %
      (song->GetMinNoteValue() * song->GetBeatsPerMeasure())) == 0);
  }

  if (beatIndex >= song->GetNoteCount()) {
    sequencer.Stop();

    if (isLooping) {
      sequencer.Play();
    }
  }

  if (sequencer.IsPlaying()) {

  }

}

void ComposerView::InitResources() {
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

ComposerView::ComposerView(uint32 mainWindowHandle)
  : mainWindowHandle(mainWindowHandle) {

  logResponderId = Logging::AddResponder([=](const std::string_view& logLine) {
    outputWindowState.AddLog(logLine);
    });

  InitResources();
}

void ComposerView::Show() {

}

void ComposerView::Hide() {

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

void ComposerView::ConditionalEnableBegin(bool condition) {
  localGuiDisabled = !condition;
  if (localGuiDisabled) {
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
  }
}

void ComposerView::ConditionalEnableEnd() {
  if (localGuiDisabled) {
    ImGui::PopStyleVar();
    ImGui::PopItemFlag();
  }
  localGuiDisabled = false;
}

void ComposerView::Render(ImVec2 canvasSize) {
  auto& sequencer = Sequencer::Get();

  glClearColor(0.5f, 0.5f, 0.5f, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  AudioGlobals::LockAudio();
  ProcessPendingActions();
  HandleInput();
  AudioGlobals::UnlockAudio();

  ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(canvasSize.x), static_cast<float>(canvasSize.y));
  ImGui::NewFrame();

  auto imGuiFont = ImGui::GetFont();
  auto& imGuiStyle = ImGui::GetStyle();
  auto defaultItemSpacing = imGuiStyle.ItemSpacing;
  auto defaultFramePadding = imGuiStyle.FramePadding;

  auto mainMenuBarHeight = 0.0f;
  if (ImGui::BeginMainMenuBar()) {
    mainMenuBarHeight = ImGui::GetWindowSize().y;

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Exit")) {
        SDL_PushEvent(&SDL_Event({ SDL_QUIT }));
      }
      ImGui::EndMenu();
    }

    auto song = sequencer.GetSong();

    if (ImGui::BeginMenu("Song")) {
      if (ImGui::MenuItem("New Song")) {
        pendingNewSong = true;
      }
      if (ImGui::MenuItem("Load Song")) {
        pendingLoadSong = true;
      }

      ConditionalEnableBegin(song != nullptr);

      if (ImGui::MenuItem("Save Song")) {
        pendingSaveSong = true;
      }

      if (ImGui::BeginMenu("Options")) {

        ImGui::PushItemWidth(75);

        int currentBpm = song->GetTempo();
        if (ImGui::InputInt("BPM", &currentBpm)) {
          // @Delay
          pendingTempo = currentBpm;
        }

        imGuiStyle.ItemSpacing.x = 4;

        int measuresToAdd = addMeasureCount;
        if (ImGui::InputInt("", &measuresToAdd)) {
          addMeasureCount = std::min(std::max(kMinMeasuresToAdd,
            static_cast<uint32>(measuresToAdd)), kMaxMeasuresToAdd);
        }

        ImGui::SameLine();

        if (ImGui::MenuItem("Add measure(s)")) {
          // @Delay
          pendingAddMeasures = addMeasureCount;
        }

        imGuiStyle.ItemSpacing.x = defaultItemSpacing.x;

        ImGui::PopItemWidth();
        ImGui::EndMenu();
      }

      ConditionalEnableEnd();

      ImGui::EndMenu();
    }

    if (song != nullptr) {
      auto instrument = song->GetInstrument();

      if (ImGui::BeginMenu("Instrument")) {
        if (ImGui::MenuItem("New Instrument")) {
          pendingNewInstrument = true;
        }
        if (ImGui::MenuItem("Load Instrument")) {
          pendingLoadInstrument = true;
        }

        ConditionalEnableBegin(instrument != nullptr);

        if (ImGui::MenuItem("Save Instrument")) {
          pendingSaveInstrument = true;
        }

        if (ImGui::MenuItem("Add Track")) {
          pendingDialog = new DialogTrack("Add Track", instrument, -1,
            new Track(GetNewTrackName(kDefaultNewTrackName)), stopButtonIconTexture);
        }

        ConditionalEnableEnd();

        ImGui::EndMenu();
      }

      ConditionalEnableBegin(instrument != nullptr);
      if (ImGui::BeginMenu("Game")) {
        if (ImGui::MenuItem("Preview")) {
          sequencer.Stop();
          View::SetCurrentView<GamePreviewView>();
        }

        ImGui::EndMenu();
      }
      ConditionalEnableEnd();
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

  // Vertically resize the main window based on console being open/closed
  float outputWindowHeight = canvasSize.y * kOutputWindowWindowScreenHeightPercentage;
  if (!wasConsoleOpen) {
    outputWindowHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
  }

  auto song = sequencer.GetSong();
  if (song != nullptr) {
    auto instrument = song->GetInstrument();

    ImVec2 sequencerCanvasSize(canvasSize.x, canvasSize.y - outputWindowHeight - mainMenuBarHeight);

    // Main window which contains the tracks on the left and the song lines on the right
    ImGui::SetNextWindowPos(ImVec2(0, mainMenuBarHeight));
    ImGui::SetNextWindowSize(sequencerCanvasSize);
    ImGui::Begin("Sequencer",
      nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
    {
      std::string songName("Song");
      char newSongName[256] = { 0 };
      strcpy(newSongName, song->GetName().c_str());
      if (ImGui::InputText(songName.c_str(), newSongName, _countof(newSongName) - 1)) {
        song->SetName(std::string(newSongName));
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
        // Leave vertical space for the measure labels in the song line area
        ImGui::NewLine();

        auto trackLabelWidth = imGuiFont->CalcTextSizeA(imGuiFont->FontSize, FLT_MAX, 0.0f, kTrackNameFormat).x;
        auto trackTotalWidth = trackLabelWidth + kHamburgerMenuWidth + defaultItemSpacing.x;

        if (instrument != nullptr) {
          imGuiStyle.FramePadding.y = imGuiStyle.ItemSpacing.y * 0.5f;
          imGuiStyle.ItemSpacing.y = 0.0f;
          char newInstrumentName[256] = { 0 };
          strcpy(newInstrumentName, instrument->GetName().c_str());
          ImGui::PushItemWidth(trackLabelWidth + kHamburgerMenuWidth);
          if (ImGui::InputText("", newInstrumentName, _countof(newInstrumentName) - 1)) {
            instrument->SetName(std::string(newInstrumentName));
          }
          ImGui::PopItemWidth();
          imGuiStyle.ItemSpacing = defaultItemSpacing;
          imGuiStyle.FramePadding = defaultFramePadding;

          // Tracks
          {
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
                  pendingDialog = new DialogTrack("Edit Track",
                    instrument, trackIndex, new Track(*track), stopButtonIconTexture);
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

          auto songWindowHovered = false;

          // Song lines
          auto parentWindowPos = ImGui::GetWindowPos();
          ImGui::SetNextWindowPos(ImVec2(parentWindowPos.x + trackTotalWidth,
            parentWindowPos.y - ImGui::GetScrollY()));

          ImVec2 songCanvasSize(scrollingCanvasSize.x - trackTotalWidth - Globals::kScrollBarWidth,
            std::max(ImGui::GetCursorPosY() + Globals::kScrollBarHeight, scrollingCanvasSize.y));

          ImGui::BeginChild("##Song",
            songCanvasSize,
            false,
            ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
          {
            songWindowHovered = ImGui::IsWindowHovered();

            // Measure numbers
            auto measureNumberPos = ImGui::GetCursorPos();
            auto beatLineBegY = 0.0f;
            for (size_t m = 0; m < song->GetNumMeasures(); ++m) {
              ImGui::SetCursorPos(measureNumberPos);
              ImGui::Text(std::to_string(m + 1).c_str());
              beatLineBegY = ImGui::GetCursorPosY();
              measureNumberPos.x += kFullBeatWidth * song->GetBeatsPerMeasure();
            }

            ImGui::NewLine();

            noteSelectedStatus.resize(song->GetLineCount());

            imGuiStyle.ItemSpacing.x = 0.0f;
            imGuiStyle.ItemSpacing.y = 0.0f;

            auto localSongIterator = song->GetIterator();

            auto beatWidth = kFullBeatWidth / sequencer.GetSubdivision();
            for (size_t lineIndex = 0; lineIndex < song->GetLineCount(); ++lineIndex) {
              auto& lineIter = localSongIterator[lineIndex];

              ImGui::NewLine();

              // Notes (displayed at current beat zoom level)
              uint32 beatStep = song->GetMinNoteValue() / sequencer.GetSubdivision();
              for (size_t beatIndex = 0; beatIndex < song->GetNoteCount(); beatIndex += beatStep) {
                ImGui::SameLine();

                // Bump the first square by 1 to the right so we can draw a 2-pixel beat line
                if (!beatIndex) {
                  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 1.0f);
                }

                auto uniqueLabel("sl#" + std::to_string(lineIndex) + ":" + std::to_string(beatIndex));

                Song::Note* note = nullptr;
                if (lineIter.cur != lineIter.end && lineIter.cur->GetBeatIndex() == beatIndex) {
                  note = &(*lineIter.cur++);
                }

                // Draw empty or filled square radio button depending on whether or not it's enabled
                auto buttonBegPos = ImGui::GetCursorPos();
                auto buttonExtent = ImVec2(beatWidth, kKeyboardKeyHeight);
                if (ImGui::SquareRadioButton(uniqueLabel.c_str(), note != nullptr, buttonExtent.x, buttonExtent.y)) {
                  // Clicking a disabled note enables it; clicking an enabled note selects it
                  if (note != nullptr) {
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

                if (note != nullptr) {
                  // Draw filled note: TODO: Remove this, should draw it once above
                  // https://trello.com/c/Ll8dgleN
                  {
                    ImVec4 noteColor = kDefaultNoteColor;
                    if (note->GetGameIndex() >= 0 && note->GetGameIndex() < _countof(kFretColors)) {
                      noteColor = kFretColors[note->GetGameIndex()];
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
            cursorPosX = kFullBeatWidth * sequencer.GetBeatTime();
            ImGui::SetCursorPos(ImVec2(cursorPosX, beatLineBegY));
            ImGui::FillRect(ImVec2(2, beatLineEndY - beatLineBegY),
              ImGui::ColorConvertFloat4ToU32(kPlayLineColor));

            // Keep it in sight while playing
            if (Sequencer::Get().IsPlaying()) {
              if (cursorPosX > ImGui::GetScrollX() + songCanvasSize.x) {
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
                  // Don't clear notes if they are beginning a selected group drag
                  if (hoveredNote.first == -1 || !set_contains<uint32>(noteSelectedStatus[hoveredNote.first], hoveredNote.second)) {
                    ClearSelectedNotes();
                  }
                }

                songWindowClicked = true;

                mouseDragBeg = ImGui::GetMousePos();
                mouseDragBeg -= ImGui::GetWindowPos();

                mouseDragBeg.x += ImGui::GetScrollX();
                mouseDragBeg.y += ImGui::GetScrollY();
              }

              // On mouse release, clear drag box
              if (ImGui::IsMouseReleased(0)) {
                songWindowClicked = false;
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
        } // if (instrument != nullptr)

        ImGui::EndChild();
      } // Track + Song vertical scroling region

      // Bottom tool bar
      {
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
        bool localIsLooping = isLooping;
        if (ImGui::Checkbox("Loop", &localIsLooping)) {
          // @Atomic
          isLooping = localIsLooping;
        }
        ImGui::PopItemWidth();

        // Metronome
        ImGui::SameLine();
        ImGui::PushItemWidth(100);
        bool localIsMetronomeOn = isMetronomeOn;
        if (ImGui::Checkbox("Metronome", &localIsMetronomeOn)) {
          // @Atomic
          isMetronomeOn = localIsMetronomeOn;
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

      ImGui::End();
    } // Sequencer window
  } // if (song != nullptr)

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
    ImGui::Text("Voices: %d", sequencer.GetNumActiveVoices());
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
