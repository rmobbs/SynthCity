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
#include "DialogOptions.h"
#include "SongTab.h"
#include "InstrumentTab.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
#include <commdlg.h>
#include <atlbase.h>
#include <commctrl.h>
#include <fstream>

static constexpr float kOutputWindowWindowScreenHeightPercentage = 0.35f;

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

ComposerView::ComposerView(HashedController<View>* viewController)
  : View({}, viewController) {
  logResponderId = Logging::AddResponder([=](const std::string_view& logLine) {
    outputWindowState.AddLog(logLine);
  });

  InitResources();

  tabController.Register(new SongTab(this));
  tabController.Register(new InstrumentTab(this));
  tabController.SetCurrent<SongTab>();
}

ComposerView::~ComposerView() {
  if (logResponderId != UINT32_MAX) {
    Logging::PopResponder(logResponderId);
    logResponderId = UINT32_MAX;
  }

  delete dialogState.second;
  dialogState = { DialogState::NoDialog, nullptr };
}

void ComposerView::SetTrackColors(Instrument* instrument, std::string paletteEntryName) {
  auto& imGuiStyle = ImGui::GetStyle();

  auto paletteEntry = instrument->GetTrackPalette().find(paletteEntryName);
  if (paletteEntry != instrument->GetTrackPalette().end()) {
    imGuiStyle.Colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(paletteEntry->
      second[static_cast<uint32>(Instrument::TrackPalette::BackgroundNormal)]);
    imGuiStyle.Colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(paletteEntry->
      second[static_cast<uint32>(Instrument::TrackPalette::BackgroundHighlighted)]);
    imGuiStyle.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(paletteEntry->
      second[static_cast<uint32>(Instrument::TrackPalette::TextNormal)]);
  }
}

void ComposerView::ShowDialog(Dialog* dialog) {
  assert(dialogState.first == DialogState::NoDialog);
  dialogState = { DialogState::Pending, dialog };
  wasPlaying = Sequencer::Get().IsPlaying();
  Sequencer::Get().PauseKill();
}

void ComposerView::HandleInput() {
  if (dialogState.first == DialogState::NoDialog && !ImGui::IsEditing()) {
    tabController.GetCurrent()->HandleInput();
  }
}

void ComposerView::DoLockedActions() {
  // End dialogs with the sequencer locked, as they can be editing shared data
  if (dialogState.first == DialogState::Finish) {
    dialogState.second->Close();
    delete dialogState.second;
    dialogState = { DialogState::NoDialog, nullptr };
    if (wasPlaying) {
      Sequencer::Get().Play();
      wasPlaying = false;
    }
  }

  if (pendingTab != kInvalidUint32) {
    tabController.SetCurrent(pendingTab);
  }
  pendingTab = kInvalidUint32;

  tabController.GetCurrent()->DoLockedActions();
}

void ComposerView::OnBeat(uint32 beatIndex) {
  auto song = Song::Get();

  if (isMetronomeOn) {
    auto mod = beatIndex % song->GetMinNoteValue();
    if (!mod) {
      Sequencer::Get().PlayMetronome((beatIndex / song->GetMinNoteValue()) % song->GetBeatsPerMeasure() == 0);
    }
  }

  tabController.Get<SongTab>()->OnBeat(beatIndex);
}

void ComposerView::InitResources() {

}

void ComposerView::Show() {
  Sequencer::Get().SetListener(this);
}

void ComposerView::Hide() {

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
  HandleInput();
  DoLockedActions();
  AudioGlobals::UnlockAudio();

  ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(canvasSize.x), static_cast<float>(canvasSize.y));
  ImGui::NewFrame();

  // Open any pending dialog
  if (dialogState.first == DialogState::Pending) {
    dialogState.second->Open();
    dialogState.first = DialogState::Active;
  }

  // Render any active dialog
  if (dialogState.first == DialogState::Active) {
    if (!dialogState.second->Render()) {
      dialogState.first = DialogState::Finish;
    }
  }

  auto mainMenuBarHeight = 0.0f;
  if (ImGui::BeginMainMenuBar()) {
    mainMenuBarHeight = ImGui::GetWindowSize().y;

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Options")) {
        ShowDialog(new DialogOptions);
      }
      if (ImGui::MenuItem("Exit")) {
        SDL_PushEvent(&SDL_Event({ SDL_QUIT }));
      }
      ImGui::EndMenu();
    }

    tabController.GetCurrent()->DoMainMenuBar();

    auto song = Song::Get();
    if (song != nullptr) {
      ConditionalEnableBegin(song->GetInstrumentInstances().size() > 0);
      if (ImGui::BeginMenu("Game")) {
        if (ImGui::MenuItem("Preview")) {
          sequencer.Stop();
          viewController->SetCurrent<GamePreviewView>();
        }

        ImGui::EndMenu();
      }
      ConditionalEnableEnd();
    }

    ImGui::EndMainMenuBar();
  }

  // Vertically resize the main window based on console being open/closed
  float outputWindowHeight = canvasSize.y * kOutputWindowWindowScreenHeightPercentage;
  if (!wasConsoleOpen) {
    outputWindowHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
  }

  ImVec2 sequencerCanvasSize(canvasSize.x, canvasSize.y - outputWindowHeight - mainMenuBarHeight);

  // Main window which contains the tracks on the left and the song lines on the right
  ImGui::SetNextWindowPos(ImVec2(0, mainMenuBarHeight));
  ImGui::SetNextWindowSize(sequencerCanvasSize);
  ImGui::Begin("Sequencer",
    nullptr,
    ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_AlwaysAutoResize |
    ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse |
    ImGuiWindowFlags_NoBringToFrontOnFocus);
  {
    ImVec2 scrollingCanvasSize(sequencerCanvasSize.x -
      Globals::kScrollBarWidth, sequencerCanvasSize.y - mainMenuBarHeight);

    ImGui::BeginTabBar("ComposerViewTabBar");

    for (auto& tab : tabController.GetAll()) {
      if (ImGui::BeginTabItem(tab.second->GetName().c_str())) {
        pendingTab = tab.first;
        ImGui::EndTabItem();
      }
    }

    tabController.GetCurrent()->Render(scrollingCanvasSize);

    ImGui::EndTabBar();
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
    ImGui::Text("Voices: %d", sequencer.GetNumActiveVoices());
    char workBuf[256];
    sprintf(workBuf, "FPS: %03.2f", 1.0f / Globals::elapsedTime);
    ImGui::SameLine();
    ImGui::Text(workBuf);
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
