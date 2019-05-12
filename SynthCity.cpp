#include "SDL.h"
#include "GL/glew.h"
#include "imgui.h"
#include <iostream>
#include <vector>
#include <tuple>
#include <stddef.h>
#include <set>
#include <queue>
#include <chrono>
#include <map>
#include <mutex>
#include <algorithm>
#include <array>
#include <string_view>
#include "MidiSource.h"

#include "BaseTypes.h"
#include "Sequencer.h"
#include "Logging.h"

#include "SDL_syswm.h"
#include <Windows.h>
#include <atlbase.h>
#include <commctrl.h>
// Stupid Windows
#undef max
#undef min
#include "resource.h"
#include <commdlg.h>

#include "soil.h"

static SDL_Window* sdlWindow = nullptr;
static constexpr uint32 kMaxKey = 256;

struct InputState {
  int mouseX = 0;
  int mouseY = 0;
  bool downL = false;
  int scroll = 0;
  std::string inputText;
  std::array<uint8, kMaxKey> wasDown;
  std::array<uint8, kMaxKey> keyDown;
};

static InputState inputState;

struct OutputWindowState {
  std::vector<std::string> displayHistory;
  bool ScrollToBottom = false;
  bool AutoScroll = true;

  void ClearLog() {
    displayHistory.clear();
  }

  void AddLog(const char* fmt, ...)
  {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
    buf[IM_ARRAYSIZE(buf) - 1] = 0;
    va_end(args);
    this->displayHistory.emplace_back(buf);
    if (AutoScroll) {
      ScrollToBottom = true;
    }
  }

  void AddLog(const std::string_view& logString) {
    displayHistory.push_back(std::string(logString));
    if (AutoScroll) {
      ScrollToBottom = true;
    }
  }
};

static OutputWindowState outputWindowState;

struct ShaderParams {
  GLint texture;
  GLint projMtx;
  GLint position;
  GLint uv;
  GLint color;
};

static ShaderParams shaderParams;

static GLuint fontTexture;
static GLuint programHandle;
static GLuint vertexShaderHandle;
static GLuint fragmentShaderHandle;
static GLuint vertexBuffer;
static GLuint elementBuffer;
static GLuint playButtonIconTexture = 0;
static GLuint stopButtonIconTexture = 0;
static GLuint pauseButtonIconTexture = 0;
static int subdivisionCount = 0;
static double currentTime = 0;
static std::map<int, double> playingTrackFlashTimes[2];
static std::map<int, double> playingNotesFlashTimes[2];
static std::mutex mutexInstrument;
static int32 pendingPlayTrack = -1;
static WNDPROC oldWindowProc = nullptr;
static bool wantQuit = false;
static SDL_SysWMinfo sysWmInfo;
static HMENU hMenu;


static constexpr uint32 kWindowWidth = 1200;
static constexpr uint32 kWindowHeight = 800;
static constexpr uint32 kSwapInterval = 1;
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
static constexpr std::string_view kJsonTag(".json");
static constexpr float kOutputWindowWindowScreenHeightPercentage = 0.35f;
static constexpr const char *kSynthCityVersion = "0.0.1";
static constexpr std::string_view kEmptyTrackName("<unknown>");

// 32 divisions per beat, viewable as 1/2,1/4,1/8,1/16
static const std::vector<uint32> TimelineDivisions = { 2, 4, 8 };

static Sequencer* sequencer = nullptr;

void ImGuiRenderDrawLists(ImDrawData* drawData) {
  // Backup GL state
  GLint lastProgram;
  glGetIntegerv(GL_CURRENT_PROGRAM, &lastProgram);
  GLint lastTexture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);
  GLint lastArrayBuffer;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &lastArrayBuffer);
  GLint lastElementArrayBuffer;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER, &lastElementArrayBuffer);
  GLboolean lastEnableBlend = glIsEnabled(GL_BLEND);
  GLboolean lastEnableCullFace = glIsEnabled(GL_CULL_FACE);
  GLboolean lastEnableDepthTest = glIsEnabled(GL_DEPTH_TEST);
  GLboolean lastEnableScissorTest = glIsEnabled(GL_SCISSOR_TEST);

  // Set our state
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_SCISSOR_TEST);
  glActiveTexture(GL_TEXTURE0);

  auto& imGuiIo = ImGui::GetIO();
  glViewport(0, 0, static_cast<GLuint>(imGuiIo.
    DisplaySize.x), static_cast<GLuint>(imGuiIo.DisplaySize.y));

  // Handle issue where screen coordinates and framebuffer coordinates are not 1:1
  auto frameBufferHeight = imGuiIo.DisplaySize.y * imGuiIo.DisplayFramebufferScale.y;
  drawData->ScaleClipRects(imGuiIo.DisplayFramebufferScale);

  // Orthographic projection matrix
  float orthoProj[4][4] = {
    {  2.0f / imGuiIo.DisplaySize.x,  0.0f,                          0.0f, 0.0f },
    {  0.0f,                         -2.0f / imGuiIo.DisplaySize.y,  0.0f, 0.0f },
    {  0.0f,                          0.0f,                         -1.0f, 0.0f },
    { -1.0f,                          1.0f,                          0.0f, 1.0f }
  };
  glUseProgram(programHandle);
  glUniform1i(shaderParams.texture, 0);
  glUniformMatrix4fv(shaderParams.projMtx, 1, GL_FALSE, &orthoProj[0][0]);

  // Run through the ImGui draw lists
  for (int n = 0; n < drawData->CmdListsCount; ++n) {
    const auto& cmdList = drawData->CmdLists[n];

    // Bind vertex buffer and set vertex data
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cmdList->VtxBuffer.size()) *
      sizeof(ImDrawVert), reinterpret_cast<GLvoid*>(&cmdList->VtxBuffer.front()), GL_STREAM_DRAW);

    // Enable and bind parameters
    glEnableVertexAttribArray(shaderParams.position);
    glVertexAttribPointer(shaderParams.position, 2, GL_FLOAT, GL_FALSE,
      sizeof(ImDrawVert), reinterpret_cast<GLvoid*>(offsetof(ImDrawVert, pos)));
    glEnableVertexAttribArray(shaderParams.uv);
    glVertexAttribPointer(shaderParams.uv, 2, GL_FLOAT, GL_FALSE,
      sizeof(ImDrawVert), reinterpret_cast<GLvoid*>(offsetof(ImDrawVert, uv)));
    glEnableVertexAttribArray(shaderParams.color);
    glVertexAttribPointer(shaderParams.color, 4, GL_UNSIGNED_BYTE, GL_TRUE,
      sizeof(ImDrawVert), reinterpret_cast<GLvoid*>(offsetof(ImDrawVert, col)));

    // Bind index buffer and set index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(cmdList->IdxBuffer.size()) *
      sizeof(ImDrawIdx), reinterpret_cast<GLvoid*>(&cmdList->IdxBuffer.front()), GL_STREAM_DRAW);

    // Run through the draw commands for this draw list
    const ImDrawIdx* drawIndex = 0;
    for (const auto& drawCmd : cmdList->CmdBuffer) {
      if (drawCmd.UserCallback != nullptr) {
        drawCmd.UserCallback(cmdList, &drawCmd);
      }
      else {
        glBindTexture(GL_TEXTURE_2D, reinterpret_cast<GLuint>(drawCmd.TextureId));
        glScissor(static_cast<int>(drawCmd.ClipRect.x),
          static_cast<int>(frameBufferHeight - drawCmd.ClipRect.w),
          static_cast<int>(drawCmd.ClipRect.z - drawCmd.ClipRect.x),
          static_cast<int>(drawCmd.ClipRect.w - drawCmd.ClipRect.y));
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(drawCmd.ElemCount), GL_UNSIGNED_SHORT, drawIndex);
      }
      drawIndex += drawCmd.ElemCount;
    }
  }

  // Restore previous GL state
  glUseProgram(lastProgram);
  glBindTexture(GL_TEXTURE_2D, lastTexture);
  glBindBuffer(GL_ARRAY_BUFFER, lastArrayBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lastElementArrayBuffer);
  if (lastEnableBlend) {
    glEnable(GL_BLEND);
  }
  else {
    glDisable(GL_BLEND);
  }
  if (lastEnableCullFace) {
    glEnable(GL_CULL_FACE);
  }
  else {
    glDisable(GL_CULL_FACE);
  }
  if (lastEnableDepthTest) {
    glEnable(GL_DEPTH_TEST);
  }
  else {
    glDisable(GL_DEPTH_TEST);
  }
  if (lastEnableScissorTest) {
    glEnable(GL_SCISSOR_TEST);
  }
  else {
    glDisable(GL_SCISSOR_TEST);
  }
}

std::shared_ptr<WCHAR[]> StringToWChar(const std::string& sourceString) {
  int bufferlen = ::MultiByteToWideChar(CP_ACP, 0, sourceString.c_str(), sourceString.length(), nullptr, 0);
  if (bufferlen > 0) {
    auto stringLen = sourceString.length();
    
    std::shared_ptr<WCHAR[]> stringBuf(new WCHAR[stringLen + 1]);
    ::MultiByteToWideChar(CP_ACP, 0, sourceString.c_str(), stringLen, stringBuf.get(), bufferlen);
    stringBuf.get()[stringLen] = 0;
    return stringBuf;
  }
  return nullptr;
}

std::shared_ptr<WCHAR[]> StringToWChar(const std::string_view& sourceString) {
  int bufferlen = ::MultiByteToWideChar(CP_ACP, 0, sourceString.data(), sourceString.length(), nullptr, 0);
  if (bufferlen > 0) {
    auto stringLen = sourceString.length();

    std::shared_ptr<WCHAR[]> stringBuf(new WCHAR[stringLen + 1]);
    ::MultiByteToWideChar(CP_ACP, 0, sourceString.data(), stringLen, stringBuf.get(), bufferlen);
    stringBuf.get()[stringLen] = 0;
    return stringBuf;
  }
  return nullptr;
}

struct MidiPropertiesDialogWorkspace {
  const MidiSource* currentMidiSource = nullptr;
  std::vector<std::shared_ptr<WCHAR[]>> treeStrings;
  std::vector<HTREEITEM> treeItems;

  Sequencer::MidiConversionParams *midiConversionParams;
};

BOOL CALLBACK MidiPropertiesDialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  auto& workspace = *reinterpret_cast<MidiPropertiesDialogWorkspace*>(GetWindowLong(sysWmInfo.info.win.window, GWL_USERDATA));
  switch (uMsg) {
    case WM_INITDIALOG: {
      SetDlgItemInt(hWndDlg, IDC_EDIT_MIDIPROPERTIES_TEMPO, workspace.currentMidiSource->getNativeTempo(), FALSE);

      HWND hWndTrackTree = GetDlgItem(hWndDlg, IDC_TREE_MIDIPROPERTIES_TRACKS);
      // Dump the track info
      const auto& midiTracks = workspace.currentMidiSource->getTracks();

      auto logTen = static_cast<uint32>(std::floor(log10(midiTracks.size())));
      for (int currIndex = 0; currIndex < midiTracks.size(); ++ currIndex) {
        const auto& midiTrack = midiTracks[currIndex];

        TVITEM tvi = { 0 };
        TVINSERTSTRUCT tvins;
        static HTREEITEM hPrevRootItem = NULL;
        static HTREEITEM hPrevLev2Item = NULL;
        HTREEITEM hti;

        tvi.mask = TVIF_TEXT | TVS_CHECKBOXES | TVIF_PARAM;

        std::string trackLabel(std::string(logTen - static_cast<uint32>
          (std::floor(log10(currIndex + 1))), '0') + std::to_string(currIndex + 1) + ": ");
        if (midiTrack.name.length() > 0) {
          trackLabel += midiTrack.name;
        }
        else {
          trackLabel += kEmptyTrackName;
        }
        workspace.treeStrings.emplace_back(StringToWChar(trackLabel));

        tvi.pszText = workspace.treeStrings.back().get();
        tvi.cchTextMax = sizeof(tvi.pszText) / sizeof(tvi.pszText[0]);
        tvi.lParam = static_cast<LPARAM>(currIndex);
          
        tvins.item = tvi;
        tvins.hParent = TVI_ROOT;

        auto hTreeItem = reinterpret_cast<HTREEITEM>
          (SendMessage(hWndTrackTree, TVM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&tvins)));
        workspace.treeItems.push_back(hTreeItem);
      }
      break;
    }
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDOK: {
          // Pull data
          workspace.midiConversionParams->tempo =
            GetDlgItemInt(hWndDlg, IDC_EDIT_MIDIPROPERTIES_TEMPO, nullptr, FALSE);

          // TODO: There has to be some way to do the validation while they're editing
          workspace.midiConversionParams->tempo = std::max(std::min(workspace.
            midiConversionParams->tempo, sequencer->GetMaxTempo()), sequencer->GetMinTempo());
          
          // Get list of checked entries
          HWND hWndTrackTree = GetDlgItem(hWndDlg, IDC_TREE_MIDIPROPERTIES_TRACKS);
          for (uint32 t = 0; t < workspace.currentMidiSource->getTrackCount(); ++t) {
            if (TreeView_GetCheckState(hWndTrackTree, workspace.treeItems[t])) {
              workspace.midiConversionParams->trackIndices.push_back(t);
            }
          }
    
          EndDialog(hWndDlg, wParam);
          return TRUE;
        }
        case IDCANCEL: {
          EndDialog(hWndDlg, wParam);
          return TRUE;
        }
        default:
          break;
      }
      break;
    case WM_NOTIFY: {
      LPNMHDR nmHeader = reinterpret_cast<LPNMHDR>(lParam);
      switch (LOWORD(wParam)) {
        case IDC_TREE_MIDIPROPERTIES_TRACKS: {
          switch (nmHeader->code) {
            case TVN_SELCHANGED: {

              LPNMTREEVIEW nmTreeView = reinterpret_cast<LPNMTREEVIEW>(lParam);

              // Format info text
              std::string trackInfo;

              const auto& midiTrack = workspace.currentMidiSource->getTracks()[nmTreeView->itemNew.lParam];
              trackInfo += "Track contains " + std::to_string(midiTrack.events.size()) + " events (" +
                std::to_string(midiTrack.metaCount) + " meta, " + std::to_string(midiTrack.messageCount) + " messages.\r\n";

              SetDlgItemText(hWndDlg, IDC_EDIT_MIDIPROPERTIES_TRACKDETAILS, StringToWChar(trackInfo).get());
              break;
            }
          }
          break;
        }
      }
      break;
    }
    case WM_CLOSE:
      workspace.treeStrings.clear();
      break;
    default:
      break;
  }
  return FALSE;
}

void OnNotePlayed(int trackIndex, int noteLocalIndex, void *payload) {
  // NOTE: Called from the SDL audio thread!
  playingTrackFlashTimes[1][trackIndex] = currentTime;
  playingNotesFlashTimes[1][trackIndex * sequencer->GetNumMeasures() * sequencer->
    GetBeatsPerMeasure() * sequencer->GetMaxSubdivisions() + noteLocalIndex] = currentTime;
}

bool GetMidiConversionParams(const MidiSource& midiSource, Sequencer::MidiConversionParams& midiConversionParams) {
  MidiPropertiesDialogWorkspace workspace;
  workspace.currentMidiSource = &midiSource;
  workspace.midiConversionParams = &midiConversionParams;
  SetWindowLong(sysWmInfo.info.win.window, GWL_USERDATA, reinterpret_cast<LONG>(&workspace));

  // Push the dialog
  if (DialogBox(sysWmInfo.info.win.hinstance, MAKEINTRESOURCE(IDD_DIALOG_MIDIPROPERTIES),
    sysWmInfo.info.win.window, reinterpret_cast<DLGPROC>(MidiPropertiesDialogProc)) == IDOK) {
    return true;
  }

  return false;
}

void UpdateSdl() {
  SDL_Event sdlEvent;

  while (SDL_PollEvent(&sdlEvent)) {
    switch (sdlEvent.type) {
      case SDL_KEYDOWN:
        if (sdlEvent.key.keysym.sym < kMaxKey) {
          inputState.keyDown[sdlEvent.key.keysym.sym] = 1;
        }
        break;
      case SDL_KEYUP:
        if (sdlEvent.key.keysym.sym < kMaxKey) {
          inputState.keyDown[sdlEvent.key.keysym.sym] = 0;
        }
        break;
      case SDL_TEXTINPUT:
        inputState.inputText = sdlEvent.text.text;
        break;
      case SDL_MOUSEMOTION:
        inputState.mouseX = sdlEvent.motion.x;
        inputState.mouseY = sdlEvent.motion.y;
        break;
      case SDL_MOUSEBUTTONDOWN:
        switch (sdlEvent.button.button) {
          case 1: // left click
            inputState.downL = true;
            break;
          case 4: // scroll up
            inputState.scroll = 1;
            break;
          case 5: // scroll down
            inputState.scroll = -1;
            break;
        }
        break;
      case SDL_MOUSEBUTTONUP:
        if (sdlEvent.button.button == 1) {
          inputState.downL = false;
        }
        break;
      case SDL_MOUSEWHEEL:
        if (sdlEvent.wheel.y > 0) {
          inputState.scroll = 1;
        }
        else if (sdlEvent.wheel.y < 0) {
          inputState.scroll = -1;
        }
        break;
      case SDL_QUIT:
        wantQuit = true;
        break;
    }
  }
}

void SetImGuiTrackColors(ImGuiStyle& imGuiStyle, std::string colorScheme, uint32& flashColor) {

  if (colorScheme.length()) {
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

void UpdateImGui() {
  // Lock the instrument for the duration
  std::lock_guard<std::mutex> lockInstrument(mutexInstrument);
  auto instrument = sequencer->GetInstrument();

  // Lock out the audio callback to update the shared data
  SDL_LockAudio();
  playingTrackFlashTimes[0] = playingTrackFlashTimes[1];
  playingNotesFlashTimes[0] = playingNotesFlashTimes[1];

  if (pendingPlayTrack != -1) {
    sequencer->PlayInstrumentTrack(pendingPlayTrack, kDefaultNoteVelocity);
    pendingPlayTrack = -1;
  }
  SDL_UnlockAudio();

  int windowWidth, windowHeight;
  SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight);

  int outputWindowHeight = windowHeight * kOutputWindowWindowScreenHeightPercentage;
  int sequencerHeight = windowHeight - outputWindowHeight;

  ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(static_cast<float>(windowWidth), static_cast<float>(sequencerHeight)));

  ImGui::Begin("Instrument", 
    nullptr, 
    //ImGuiWindowFlags_NoTitleBar | 
    ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
  {
    auto& imGuiStyle = ImGui::GetStyle();
    auto oldItemSpacing = imGuiStyle.ItemSpacing;

    if (instrument != nullptr) {
      ImGui::Text("Instrument: %s", instrument->GetName().c_str());
    }
    else {
      ImGui::Text("Instrument: None");
    }

    ImGui::NewLine();
    ImGui::Separator();

    auto beatWidth = kFullBeatWidth / sequencer->GetSubdivision();

    // Start of the beat label
    float beatLabelStartX = ImGui::GetCursorPosX() + kKeyboardKeyWidth + imGuiStyle.ItemSpacing.x;

    // Beat numbers
    float cursorPosX = beatLabelStartX;
    for (size_t b = 0; b < sequencer->GetNumMeasures() * sequencer->GetBeatsPerMeasure(); ++b) {
      ImGui::SetCursorPosX(cursorPosX);
      cursorPosX += kFullBeatWidth;
      ImGui::Text(std::to_string(b + 1).c_str());
      ImGui::SameLine();
    }

    ImGui::NewLine();
    ImGui::Separator();

    auto beatLabelStartY = ImGui::GetCursorPosY();

    // Tracks
    if (instrument != nullptr) {
      uint32 noteGlobalIndex = 0;
      for (uint32 trackIndex = 0; trackIndex < instrument->GetTracks().size(); ++ trackIndex) {
        auto& track = instrument->GetTracks()[trackIndex];

        // Track label and UI button for manual trigger
        ImVec4 oldColors[ImGuiCol_COUNT];
        memcpy(oldColors, imGuiStyle.Colors, sizeof(oldColors));

        uint32 flashColor = kPlayTrackFlashColor;
        SetImGuiTrackColors(imGuiStyle, track.GetColorScheme(), flashColor);

        auto prevPos = ImGui::GetCursorPos();
        if (ImGui::Button(track.GetName().
          c_str(), ImVec2(kKeyboardKeyWidth, kKeyboardKeyHeight))) {
          // Do it next frame so we only lock audio once per UI loop
          pendingPlayTrack = trackIndex;
        }
        auto currPos = ImGui::GetCursorPos();

        // If it's playing, flash it
        float flashPct = 0.0f;
        {
          auto flashTime = playingTrackFlashTimes[0].find(trackIndex);
          if (flashTime != playingTrackFlashTimes[0].end()) {
            auto pct = static_cast<float>((currentTime - flashTime->second) / kPlayTrackFlashDuration);
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
        for (size_t b = 0; b < sequencer->GetNumMeasures() * sequencer->GetBeatsPerMeasure(); ++b) {
          // Notes
          for (size_t s = 0; s < sequencer->GetSubdivision(); ++s) {
            ImGui::SameLine();

            // Lesson learned: labels are required to pair UI with UX
            auto uniqueLabel(track.GetName() + std::to_string(noteLocalIndex));

            auto trackNote = track.GetNotes()[noteLocalIndex];
            auto cursorPos = ImGui::GetCursorPos();

            // Toggle notes that are clicked
            imGuiStyle.ItemSpacing.x = 0.0f;
            imGuiStyle.ItemSpacing.y = 0.0f;
            if (ImGui::SquareRadioButton(uniqueLabel.c_str(), trackNote != 0, beatWidth, kKeyboardKeyHeight)) {
              if (trackNote != 0) {
                trackNote = 0;
              }
              else {
                trackNote = kDefaultNoteVelocity;
              }

              // Can only set notes via the sequencer
              sequencer->SetTrackNote(trackIndex, noteLocalIndex, trackNote);
            }

            // Draw filled note
            if (trackNote != 0) {
              auto currentPos = ImGui::GetCursorPos();

              ImGui::SetCursorPos(cursorPos);
              ImGui::FillRect(ImVec2(beatWidth, kKeyboardKeyHeight), 0xFFFFFFFF);

              // If it's playing, flash it
              float flashPct = 0.0f;
              {
                auto flashTime = playingNotesFlashTimes[0].find(noteGlobalIndex);
                if (flashTime != playingNotesFlashTimes[0].end()) {
                  auto pct = static_cast<float>((currentTime - flashTime->second) / kPlayNoteFlashDuration);
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

            noteLocalIndex += sequencer->GetMaxSubdivisions() / sequencer->GetSubdivision();
            noteGlobalIndex += sequencer->GetMaxSubdivisions() / sequencer->GetSubdivision();
          }
        }

        // Reset old X spacing to offset from keyboard key
        imGuiStyle.ItemSpacing.x = oldItemSpacing.x;
      }
    }

    imGuiStyle.ItemSpacing = oldItemSpacing;

    float beatLabelEndY = ImGui::GetCursorPosY() - imGuiStyle.ItemSpacing.y;

    ImGui::Separator();
    ImGui::NewLine();

    // Bottom 'toolbar'
    {
      imGuiStyle.ItemSpacing.x = 0;

      // Play/Pause button
      if (sequencer->IsPlaying()) {
        if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(pauseButtonIconTexture), ImVec2(20, 20))) {
          sequencer->Pause();
        }
      }
      else {
        if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(playButtonIconTexture), ImVec2(20, 20))) {
          sequencer->Play();
        }
      }

      ImGui::SameLine();

      // Stop button
      if (ImGui::ImageButton(reinterpret_cast<ImTextureID>(stopButtonIconTexture), ImVec2(20, 20))) {
        sequencer->Stop();
      }

      imGuiStyle.ItemSpacing.x = oldItemSpacing.x + 5;

      // Measures
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      int numMeasures = sequencer->GetNumMeasures();
      if (ImGui::InputInt("Measures", &numMeasures)) {
        numMeasures = std::max(std::min(static_cast<uint32>(numMeasures), kMaxMeasures), kMinMeasures);
        sequencer->SetNumMeasures(numMeasures);
      }
      ImGui::PopItemWidth();

      // Beats per measure
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      int beatsPerMeasure = sequencer->GetBeatsPerMeasure();
      if (ImGui::InputInt("BeatsPerMeasure", &beatsPerMeasure)) {
        beatsPerMeasure = std::max(std::min(static_cast<uint32>
          (beatsPerMeasure), kMaxBeatsPerMeasure), kMinBeatsPerMeasure);
        sequencer->SetBeatsPerMeasure(beatsPerMeasure);
      }
      ImGui::PopItemWidth();

      // Subdivision
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      if (ImGui::BeginCombo("Subdivision", std::to_string(sequencer->GetSubdivision()).c_str())) {
        for (size_t s = 0; s < TimelineDivisions.size(); ++s) {
          bool isSelected = (sequencer->GetSubdivision() == TimelineDivisions[s]);
          if (ImGui::Selectable(std::to_string(TimelineDivisions[s]).c_str(), isSelected)) {
            sequencer->SetSubdivision(TimelineDivisions[s]);
          }
          else {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::PopItemWidth();

      // BPM
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      int currentBpm = sequencer->GetBeatsPerMinute();
      if (ImGui::InputInt("BPM", &currentBpm)) {
        sequencer->SetBeatsPerMinute(currentBpm);
      }
      ImGui::PopItemWidth();

      // Loop
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      bool isLooping = sequencer->IsLooping();
      if (ImGui::Checkbox("Loop", &isLooping)) {
        sequencer->SetLooping(isLooping);
      }
      ImGui::PopItemWidth();

      // Metronome
      ImGui::SameLine();
      ImGui::PushItemWidth(100);
      bool isMetronomeOn = sequencer->IsMetronomeOn();
      if (ImGui::Checkbox("Metronome", &isMetronomeOn)) {
        sequencer->EnableMetronome(isMetronomeOn);
      }
      ImGui::PopItemWidth();
    }

    auto oldCursorPos = ImGui::GetCursorPos();

    // Draw the beat demarcation lines
    cursorPosX = beatLabelStartX;
    for (uint32 b = 0; b < sequencer->GetNumMeasures() * sequencer->GetBeatsPerMeasure(); ++b) {
      ImGui::SetCursorPos(ImVec2(cursorPosX - 0, beatLabelStartY));
      ImGui::FillRect(ImVec2(1, beatLabelEndY - 
        beatLabelStartY), ImGui::GetColorU32(ImGuiCol_FrameBgActive));
      cursorPosX += kFullBeatWidth;
    }

    imGuiStyle.ItemSpacing = oldItemSpacing;

    // Draw the play line
    cursorPosX = beatLabelStartX + beatWidth * (sequencer->GetPosition() / 
      (sequencer->GetMaxSubdivisions() / sequencer->GetSubdivision()));
    ImGui::SetCursorPos(ImVec2(cursorPosX - 0, beatLabelStartY));
    ImGui::FillRect(ImVec2(1, beatLabelEndY - beatLabelStartY), 0x7FFFFFFF);

    ImGui::SetCursorPos(oldCursorPos);
  }
  ImGui::End();

  // Output window
  int outputWindowTop = windowHeight - outputWindowHeight;
  ImGui::SetNextWindowPos(ImVec2(0, outputWindowTop));
  ImGui::SetNextWindowSize(ImVec2(windowWidth, outputWindowHeight));
  ImGui::Begin("Output",
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
        outputWindowState.ScrollToBottom = true;
      }
      if (ImGui::Checkbox("Auto-scroll", &outputWindowState.AutoScroll)) {
        if (outputWindowState.AutoScroll) {
          outputWindowState.ScrollToBottom = true;
        }
      }
      ImGui::EndPopup();
    }

    if (ImGui::SmallButton("=")) {
      ImGui::OpenPopup("Options");
    }
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
    if (outputWindowState.ScrollToBottom) {
      ImGui::SetScrollY(ImGui::GetScrollMaxY());
      ImGui::SetScrollHere(1.0f);
      outputWindowState.ScrollToBottom = false;
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
  }

  ImGui::End();
}

void MainLoop() {
  UpdateSdl();

  if (sequencer->GetInstrument()) {
    EnableMenuItem(hMenu, ID_FILE_LOADSONG, MF_ENABLED);
    EnableMenuItem(hMenu, ID_FILE_SAVESONG, MF_ENABLED);
  }
  else {
    EnableMenuItem(hMenu, ID_FILE_LOADSONG, MF_DISABLED);
    EnableMenuItem(hMenu, ID_FILE_SAVESONG, MF_DISABLED);
  }

  glClearColor(0.0f, 0.0f, 0.7f, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGuiIO& imGuiIo = ImGui::GetIO();

  // Update time
  currentTime = static_cast<double>(SDL_GetTicks()) / 1000.0;
  static double lastUpdateTime = 0.0;
  imGuiIo.DeltaTime = static_cast<float>(currentTime - lastUpdateTime);
  lastUpdateTime = currentTime;

  // Update mouse
  imGuiIo.MousePos = ImVec2(static_cast<float>(inputState.mouseX), static_cast<float>(inputState.mouseY));
  imGuiIo.MouseDown[0] = inputState.downL;
  imGuiIo.MouseDown[1] = false;
  imGuiIo.MouseWheel += static_cast<float>(inputState.scroll) * 0.5f;

  if (inputState.inputText.length()) {
    imGuiIo.AddInputCharactersUTF8(inputState.inputText.c_str());
    inputState.inputText.clear();
  }
  inputState.scroll = 0;

  for (auto k = 0; k < kMaxKey; ++k) {
    imGuiIo.KeysDown[k] = inputState.keyDown[k] != 0;
  }

  auto isKeyPress = [=](uint8 keyValue) {
    return (inputState.keyDown[keyValue] && !inputState.wasDown[keyValue]);
  };

  // Key events
  if (isKeyPress(SDLK_SPACE)) {
    imGuiIo.MouseDown[0] = true;
  }
  inputState.wasDown = inputState.keyDown;

  imGuiIo.KeyShift = (SDL_GetModState() & KMOD_SHIFT) != 0;
  imGuiIo.KeyCtrl = (SDL_GetModState() & KMOD_CTRL) != 0;
  imGuiIo.KeyAlt = (SDL_GetModState() & KMOD_ALT) != 0;

  UpdateImGui();
  
  ImGui::Render();

  SDL_GL_SwapWindow(sdlWindow);
}

bool InitImGui() {
  ImGui::CreateContext();

  auto& imGuiIo = ImGui::GetIO();

  // Basics
  imGuiIo.DeltaTime = 1.0f / 60.0f;
  imGuiIo.IniFilename = nullptr;
  
  imGuiIo.KeyMap[ImGuiKey_Tab] = SDLK_TAB;
  imGuiIo.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
  imGuiIo.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
  imGuiIo.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
  imGuiIo.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
  imGuiIo.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
  imGuiIo.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
  imGuiIo.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
  imGuiIo.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
  imGuiIo.KeyMap[ImGuiKey_Delete] = SDLK_DELETE;
  imGuiIo.KeyMap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
  imGuiIo.KeyMap[ImGuiKey_Enter] = SDLK_RETURN;
  imGuiIo.KeyMap[ImGuiKey_Escape] = SDLK_ESCAPE;
  imGuiIo.RenderDrawListsFn = ImGuiRenderDrawLists;

  return true;
}

bool InitFont() {
  auto& imGuiIo = ImGui::GetIO();

  // Build ImGui font texture atlas and get the raw pixel data
  uchar* pixels;
  int width, height;

  // Create GL texture for font
  glGenTextures(1, &fontTexture);
  glBindTexture(GL_TEXTURE_2D, fontTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  imGuiIo.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  imGuiIo.Fonts->TexID = reinterpret_cast<ImTextureID>(fontTexture);
  imGuiIo.Fonts->ClearInputData();
  imGuiIo.Fonts->ClearTexData();

  // Load UI textures
  glGenTextures(1, &playButtonIconTexture);
  glBindTexture(GL_TEXTURE_2D, playButtonIconTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  uint8 *iconData = SOIL_load_image("Assets\\icon_play.png", &width, &height, 0, SOIL_LOAD_RGBA);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconData);
  SOIL_free_image_data(iconData);

  glGenTextures(1, &pauseButtonIconTexture);
  glBindTexture(GL_TEXTURE_2D, pauseButtonIconTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  iconData = SOIL_load_image("Assets\\icon_pause.png", &width, &height, 0, SOIL_LOAD_RGBA);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconData);
  SOIL_free_image_data(iconData);

  // Set font texture ID
  glGenTextures(1, &stopButtonIconTexture);
  glBindTexture(GL_TEXTURE_2D, stopButtonIconTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  iconData = SOIL_load_image("Assets\\icon_stop.png", &width, &height, 0, SOIL_LOAD_RGBA);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, iconData);
  SOIL_free_image_data(iconData);

  return true;
}

bool InitGL() {
  // Backup GL state
  GLint lastTexture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);
  GLint lastArrayBuffer;
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &lastArrayBuffer);

  // Vertex shader
  const GLchar* vertexShader =
    "uniform mat4 ProjMtx;\n"
    "attribute vec2 Position;\n"
    "attribute vec2 UV;\n"
    "attribute vec4 Color;\n"
    "varying vec2 Frag_UV;\n"
    "varying vec4 Frag_Color;\n"
    "void main()\n"
    "{\n"
    "	Frag_UV = UV;\n"
    "	Frag_Color = Color;\n"
    "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
    "}\n";

  // Fragment shader
  const GLchar* fragmentShader =
    "uniform sampler2D Texture;\n"
    "varying vec2 Frag_UV;\n"
    "varying vec4 Frag_Color;\n"
    "void main()\n"
    "{\n"
    "	gl_FragColor = Frag_Color * texture2D( Texture, Frag_UV.st);\n"
    "}\n";

  // Create, bind, compile, and link shaders
  programHandle = glCreateProgram();
  vertexShaderHandle = glCreateShader(GL_VERTEX_SHADER);
  fragmentShaderHandle = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(vertexShaderHandle, 1, &vertexShader, 0);
  glShaderSource(fragmentShaderHandle, 1, &fragmentShader, 0);
  glCompileShader(vertexShaderHandle);
  glCompileShader(fragmentShaderHandle);
  glAttachShader(programHandle, vertexShaderHandle);
  glAttachShader(programHandle, fragmentShaderHandle);
  glLinkProgram(programHandle);

  // Get shader params
  shaderParams.projMtx = glGetUniformLocation(programHandle, "ProjMtx");
  shaderParams.position = glGetAttribLocation(programHandle, "Position");
  shaderParams.uv = glGetAttribLocation(programHandle, "UV");
  shaderParams.color = glGetAttribLocation(programHandle, "Color");
  shaderParams.texture = glGetUniformLocation(programHandle, "Texture");

  // Generate VB and EB
  glGenBuffers(1, &vertexBuffer);
  glGenBuffers(1, &elementBuffer);

  InitFont();

  // Restore original state
  glBindTexture(GL_TEXTURE_2D, lastTexture);
  glBindBuffer(GL_ARRAY_BUFFER, lastArrayBuffer);

  return true;
}

bool LoadInstrument(std::string instrumentName) {
  WCHAR szFile[FILENAME_MAX] = { 0 };
  OPENFILENAME ofn = { 0 };

  USES_CONVERSION;
  ofn.lStructSize = sizeof(ofn);

  std::string windowTitle("Open instrument");
  if (instrumentName.length() != 0) {
    windowTitle += " \'" + instrumentName + "\'";
  }
  ofn.lpstrTitle = A2W(windowTitle.c_str());
  ofn.hwndOwner = sysWmInfo.info.win.window;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
  ofn.lpstrFilter = _TEXT("XML\0*.xml\0");
  ofn.nFilterIndex = 0;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileName(&ofn)) {
    std::lock_guard<std::mutex> lockInstrument(mutexInstrument);
    return sequencer->LoadInstrument(std::string(W2A(szFile)), instrumentName);
  }
  return false;
}

LRESULT CALLBACK MyWindowProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam) {
  switch (uMsg) {
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case ID_FILE_EXIT:
          wantQuit = true;
          return 0;
        case ID_ACCELERATOR_LOAD_INSTRUMENT:
        case ID_FILE_LOADINSTRUMENT: {
          LoadInstrument({});
          return 0;
        }
        case ID_ACCELERATOR_LOAD_SONG:
        case ID_FILE_LOADSONG: {
          //if (sequencer->GetInstrument() != nullptr) 
          {
            WCHAR szFile[FILENAME_MAX] = { 0 };
            OPENFILENAME ofn = { 0 };

            USES_CONVERSION;
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
            ofn.lpstrFilter = _TEXT("JSON\0*.json\0MIDI\0*.midi;*.mid\0");
            ofn.nFilterIndex = 0;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
              sequencer->LoadSong(std::string(W2A(szFile)));
            }
          }
          return 0;
        }
        case ID_ACCELERATOR_SAVE_SONG:
        case ID_FILE_SAVESONG: {
          if (sequencer->GetInstrument() != nullptr) {
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
              sequencer->SaveSong(fileName);
            }
          }
          return 0;
        }
      }
  }
  return oldWindowProc(hWnd, uMsg, wParam, lParam);
}

static HACCEL hAccel;

bool Init() {
  Logging::AddResponder([](const std::string_view& logLine) {
    outputWindowState.AddLog(logLine);
  });

  MCLOG(Info, "SynthCity %s", kSynthCityVersion);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0) {
    MCLOG(Fatal, "Failed to init video: %s", SDL_GetError());
    SDL_Quit();
    return false;
  }

  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  sdlWindow = SDL_CreateWindow("SynthCity", 
    SDL_WINDOWPOS_CENTERED, 
    SDL_WINDOWPOS_CENTERED, 
    kWindowWidth, 
    kWindowHeight, 
    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

  SDL_VERSION(&sysWmInfo.version);
  if (SDL_GetWindowWMInfo(sdlWindow, &sysWmInfo)) {
    hMenu = ::LoadMenu(nullptr, MAKEINTRESOURCE(IDR_FILEMENU));
    SetMenu(sysWmInfo.info.win.window, hMenu);
  }

  hAccel = LoadAccelerators(sysWmInfo.info.
    win.hinstance, MAKEINTRESOURCE(IDR_ACCELERATOR_FILEMENU));
  SDL_SetWindowsMessageHook([](void *payload, void *hWnd, unsigned int message, Uint64 wParam, Sint64 lParam) {
    MSG msg = { 0 };
    msg.hwnd = reinterpret_cast<HWND>(hWnd);
    msg.lParam = lParam;
    msg.message = message;
    msg.wParam = wParam;
    TranslateAccelerator(sysWmInfo.info.win.window, hAccel, &msg);
  }, nullptr);

  oldWindowProc = reinterpret_cast<WNDPROC>(GetWindowLong(sysWmInfo.info.win.window, GWL_WNDPROC));
  SetWindowLong(sysWmInfo.info.win.window, GWL_WNDPROC, reinterpret_cast<LONG>(MyWindowProc));

  sequencer = new Sequencer;

  if (!sequencer->Init(kDefaultNumMeasures, kDefaultBeatsPerMeasure,
    kDefaultBpm, TimelineDivisions.back(), kDefaultSubdivisions)) {
    MCLOG(Fatal, "Unable to initialize mixer");
    SDL_Quit();
    return false;
  }

  sequencer->SetLoadInstrumentCallback(
    [](std::string instrumentName) {
      return LoadInstrument(instrumentName);
  });
  sequencer->SetMidiConversionParamsCallback(
    [](const MidiSource& midiSource, Sequencer::MidiConversionParams& midiConversionParams) {
    return GetMidiConversionParams(midiSource, midiConversionParams);
  });
  sequencer->SetNotePlayedCallback(OnNotePlayed, nullptr);

  SDL_GL_CreateContext(sdlWindow);
  SDL_GL_SetSwapInterval(kSwapInterval);
  glViewport(0, 0, kWindowWidth, kWindowHeight);
  glewInit();

  InitImGui();
  InitGL();

  return true;
}

void Term() {
  ImGui::DestroyContext();

  SDL_LockAudio();
  delete sequencer;
  sequencer = nullptr;
  SDL_UnlockAudio();

  SDL_Quit();
}

void AtExit() {
  //_CrtDumpMemoryLeaks();
}

int main(int argc, char **argv) {
  atexit(AtExit);

  //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // tells leak detector to dump report at any program exit
  //_CrtSetBreakAlloc(161); 

  if (!Init()) {
    return -1;
  }

  for (;;) {
    if (wantQuit)
      break;

    MainLoop();
  }

  Term();

  return 0;
}
