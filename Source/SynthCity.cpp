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
#include "ShaderProgram.h"
#include "GlobalRenderData.h"
#include "OddsAndEnds.h"
#include "Renderable.h"
#include "ComposerView.h"
#include "InputState.h"
#include "SynthSound.h"
#include "Mixer.h"
#include "SerializeImpl.h"
#include "Instrument.h"
#include "WavSound.h"
#include "DialogPage.h"
#include "SoundFactory.h"
#include "Globals.h"

// This should go away when we move to data abstraction
#include "SpriteRenderable.h"

#include "SDL_syswm.h"
#include <windows.h>
#include <windowsx.h> // for GET_X_LPARAM and GET_Y_LPARAM
#include <atlbase.h>
#include <commctrl.h>
// Stupid Windows
#undef max
#undef min
#include "resource.h"
#include <commdlg.h>
#include <istream>
#include <fstream>
#include <ios>

#include "soil.h"

static SDL_Window* sdlWindow = nullptr;

static constexpr uint32 kDefaultNumMeasures = 2;
static constexpr uint32 kDefaultBeatsPerMeasure = 4;
static constexpr uint32 kDefaultSubdivisions = 4;
static constexpr uint32 kDefaultBpm = 120;
static const std::vector<uint32> TimelineDivisions = { 2, 4, 8 };
static constexpr std::string_view kDefaultNewTrackName("NewTrack");
static constexpr int kAudioBufferSize = 2048;

static double currentTime = 0;
static WNDPROC oldWindowProc = nullptr;
static bool wantQuit = false;
static SDL_SysWMinfo sysWmInfo;
static HMENU hMenu;
static HACCEL hAccel;
static HICON hMainWindowIcon;

static constexpr uint32 kWindowWidth = 1200;
static constexpr uint32 kWindowHeight = 800;
static constexpr uint32 kSwapInterval = 1;
static constexpr std::string_view kJsonTag(".json");
static constexpr std::string_view kEmptyTrackName("<unknown>");

static ComposerView* currentView = nullptr;

#define SYNTHCITY_WM_MIDIPROPERTIES_TREE_CHECKSTATECHANGED (WM_APP + 1)

struct MidiPropertiesDialogWorkspace {
  const MidiSource* currentMidiSource = nullptr;
  std::vector<std::shared_ptr<WCHAR[]>> treeStrings;
  std::vector<HTREEITEM> treeItems;

  Sequencer::MidiConversionParams* midiConversionParams;
};

BOOL CALLBACK MidiPropertiesDialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  auto& sequencer = Sequencer::Get();

  auto& workspace = *reinterpret_cast<MidiPropertiesDialogWorkspace*>(GetWindowLong(sysWmInfo.info.win.window, GWL_USERDATA));
  switch (uMsg) {
    case WM_INITDIALOG: {
      SetDlgItemInt(hWndDlg, IDC_EDIT_MIDIPROPERTIES_TEMPO, workspace.currentMidiSource->getNativeTempo(), FALSE);

      HWND hWndTrackTree = GetDlgItem(hWndDlg, IDC_TREE_MIDIPROPERTIES_TRACKS);
      // Dump the track info
      const auto& midiTracks = workspace.currentMidiSource->getTracks();

      // Nice formatting for numbers < 10
      auto logTen = static_cast<uint32>(std::floor(log10(midiTracks.size())));
      for (uint32 currIndex = 0; currIndex < midiTracks.size(); ++currIndex) {
        const auto& midiTrack = midiTracks[currIndex];

        TVITEM tvi = { 0 };
        TVINSERTSTRUCT tvins;
        static HTREEITEM hPrevRootItem = NULL;
        static HTREEITEM hPrevLev2Item = NULL;

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
            midiConversionParams->tempo, sequencer.GetMaxTempo()), sequencer.GetMinTempo());

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

      // Windows does not send a message when a user clicks an item in a tree view with TVS_CHECKBOXES style ... !!!
      if ((nmHeader->code == NM_CLICK) && (nmHeader->idFrom == IDC_TREE_MIDIPROPERTIES_TRACKS)) {
        auto dwPos = GetMessagePos();

        TVHITTESTINFO hitTestInfo = { 0 };
        hitTestInfo.pt.x = GET_X_LPARAM(dwPos);
        hitTestInfo.pt.y = GET_Y_LPARAM(dwPos);
        MapWindowPoints(HWND_DESKTOP, nmHeader->hwndFrom, &hitTestInfo.pt, 1);

        TreeView_HitTest(nmHeader->hwndFrom, &hitTestInfo);
        if (TVHT_ONITEMSTATEICON & hitTestInfo.flags) {
          PostMessage(hWndDlg, SYNTHCITY_WM_MIDIPROPERTIES_TREE_CHECKSTATECHANGED, 0, reinterpret_cast<LPARAM>(hitTestInfo.hItem));
        }
      }
      switch (LOWORD(wParam)) {
        case IDC_TREE_MIDIPROPERTIES_TRACKS: {
          switch (nmHeader->code) {
            case TVN_SELCHANGED: {

              LPNMTREEVIEW nmTreeView = reinterpret_cast<LPNMTREEVIEW>(lParam);

              // Format info text
              std::string trackInfo;

              const auto& midiTrack = workspace.currentMidiSource->getTracks()[nmTreeView->itemNew.lParam];
              trackInfo += "Track contains " + std::to_string(midiTrack.events.size()) + " events (" +
                std::to_string(midiTrack.metaCount) + " meta, " + std::to_string(midiTrack.messageCount) + " message).\r\n";

              SetDlgItemText(hWndDlg, IDC_EDIT_MIDIPROPERTIES_TRACKDETAILS, StringToWChar(trackInfo).get());
              break;
            }
        }
        break;
      }
    }
    break;
  }
  // Check all you want, but select what you check
  case SYNTHCITY_WM_MIDIPROPERTIES_TREE_CHECKSTATECHANGED: {
    auto hItemChanged = reinterpret_cast<HTREEITEM>(lParam);
    HWND hWndTrackTree = GetDlgItem(hWndDlg, IDC_TREE_MIDIPROPERTIES_TRACKS);
    if (TreeView_GetCheckState(hWndTrackTree, hItemChanged)) {
      // TODO: If current item is checked already, unify the selection and display
      TreeView_SelectItem(hWndTrackTree, hItemChanged);
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

bool GetMidiConversionParams(const MidiSource& midiSource, Sequencer::MidiConversionParams& midiConversionParams) {
  MidiPropertiesDialogWorkspace workspace;
  workspace.currentMidiSource = &midiSource;
  workspace.midiConversionParams = &midiConversionParams;
  SetWindowLong(sysWmInfo.info.win.window, GWL_USERDATA, reinterpret_cast<LONG>(&workspace));

  // Push the dialog
  if (DialogBox(sysWmInfo.info.win.hinstance, MAKEINTRESOURCE(IDD_MIDIPROPERTIES),
    sysWmInfo.info.win.window, reinterpret_cast<DLGPROC>(MidiPropertiesDialogProc)) == IDOK) {
    return true;
  }

  return false;
}

struct AddTrackWorkspace {
  std::vector<const SoundFactory::SoundInformation*> comboBoxEntries;
  std::vector<DialogPage*> dialogPages;

  // Name
  std::string trackName;
  std::string colorScheme;

  // Serialization of sound type
  rapidjson::StringBuffer sb;

  // Selected item index
  uint32 selectedIndex = 0;
};

void SetDialogPage(HWND hWndDlg, AddTrackWorkspace& workspace, uint32 newIndex) {
  if (workspace.selectedIndex != newIndex) {
    ShowWindow(workspace.dialogPages[workspace.selectedIndex]->GetHandle(), SW_HIDE);
  }

  // Create dialog page via factory
  if (workspace.dialogPages.size() <= newIndex) {
    workspace.dialogPages.resize(newIndex + 1);
  }
  if (workspace.dialogPages[newIndex] == nullptr) {
    auto& soundInfo = workspace.comboBoxEntries[newIndex];
    workspace.dialogPages[newIndex] =
      soundInfo->pageFactory(sysWmInfo.info.win.hinstance, hWndDlg);
  }

  auto dialogPage = workspace.dialogPages[newIndex];
  if (dialogPage != nullptr) {
    // Update properties display
    RECT rect;
    GetWindowRect(GetDlgItem(hWndDlg, IDC_PROPERTIES_AREA), &rect);
    SetWindowPos(dialogPage->GetHandle(), 0, rect.left, rect.top,
      rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW);
  }
}

BOOL CALLBACK AddSynthVoiceDialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  auto& sequencer = Sequencer::Get();
  auto& workspace = *reinterpret_cast<AddTrackWorkspace*>(GetWindowLong(sysWmInfo.info.win.window, GWL_USERDATA));

  switch (uMsg) {
    case WM_INITDIALOG: {
      auto instrument = Sequencer::Get().GetInstrument();

      // Pick an available name
      std::string defaultName(kDefaultNewTrackName);
      // Just feels weird and shameful to not have an upper bounds ...
      for (int nameSuffix = 1; nameSuffix < 1000; ++nameSuffix) {

        auto& tracks = instrument->GetTracks();

        uint32 index;
        for (index = 0; index < tracks.size(); ++index) {
          if (tracks[index].GetName() == defaultName) {
            break;
          }
        }

        if (index >= tracks.size()) {
          break;
        }

        defaultName = std::string(kDefaultNewTrackName) + std::to_string(nameSuffix);
      }

      SetDlgItemText(hWndDlg, IDC_EDIT_ADDTRACK_NAME, StringToWChar(defaultName).get());

      // Add all factory types to the combo
      HWND hWndCombo = GetDlgItem(hWndDlg, IDC_COMBO_ADDTRACK_PRESETS);

      const auto& soundInfoMap = SoundFactory::GetInfoMap();
      for (const auto& soundInfo : soundInfoMap) {
        workspace.comboBoxEntries.push_back(&soundInfo.second);
        SendMessage(hWndCombo, CB_ADDSTRING, 0,
          reinterpret_cast<LPARAM>(StringToWChar(soundInfo.second.name).get()));
      }

      // Choose the default
      SendMessage(hWndCombo, CB_SETCURSEL, 0, 0);
      SetDialogPage(hWndDlg, workspace, 0);
      break;
    }
    case WM_COMMAND:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        uint32 selectedIndex = SendMessage(reinterpret_cast<HWND>(lParam), CB_GETCURSEL, 0, 0);
        assert(selectedIndex < workspace.comboBoxEntries.size());
        SetDialogPage(hWndDlg, workspace, selectedIndex);
        workspace.selectedIndex = selectedIndex;
      }
      else {
        switch (LOWORD(wParam)) {
          case IDOK: {
            auto dialogPage = workspace.dialogPages[workspace.selectedIndex];
            if (dialogPage != nullptr) {
              // TODO: Just use Track::Serialize

              // Pull data and serialize it
              rapidjson::PrettyWriter<rapidjson::StringBuffer> w(workspace.sb);

              // Track name
              WCHAR trackNameBuf[256];
              GetDlgItemText(hWndDlg, IDC_EDIT_ADDTRACK_NAME, trackNameBuf, _countof(trackNameBuf));
              USES_CONVERSION;
              workspace.trackName = std::string(W2A(trackNameBuf));

              // TODO: Color scheme
              w.StartObject();

              dialogPage->SerializeWrite({ w });

              w.EndObject();
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
      }
    case WM_WINDOWPOSCHANGED:
    case WM_MOVE: {      
      if (workspace.dialogPages.size() > workspace.selectedIndex) {
        auto dialogPage = workspace.dialogPages[workspace.selectedIndex];
        if (dialogPage != nullptr) {
          // Update properties display
          RECT rect;
          GetWindowRect(GetDlgItem(hWndDlg, IDC_PROPERTIES_AREA), &rect);
          SetWindowPos(dialogPage->GetHandle(), 0, rect.left, rect.top,
            rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW);
        }
      }
      break;
    }
    case WM_CLOSE: {
      break;
    }
    case WM_DESTROY: {
      for (auto& dialogPage : workspace.dialogPages) {
        if (dialogPage != nullptr) {
          EndDialog(dialogPage->GetHandle(), 0);
        }
      }
      break;
    }
    default:
      break;
  }
  return FALSE;
}

bool AddSynthVoiceDialog() {
  AddTrackWorkspace workspace;
  // Pass data
  SetWindowLong(sysWmInfo.info.win.window, GWL_USERDATA, reinterpret_cast<LONG>(&workspace));

  // Push the dialog
  if (DialogBox(sysWmInfo.info.win.hinstance, MAKEINTRESOURCE(IDD_INSTRUMENT_ADDTRACK),
    sysWmInfo.info.win.window, reinterpret_cast<DLGPROC>(AddSynthVoiceDialogProc)) == IDOK) {
    
    auto soundInfo = workspace.comboBoxEntries[workspace.selectedIndex];

    MCLOG(Info, "You selected sound type %s", soundInfo->name.c_str());

    rapidjson::Document document;
    document.Parse(workspace.sb.GetString());

    Sequencer::Get().GetInstrument()->AddTrack(workspace.trackName,
      workspace.colorScheme,
      soundInfo->soundFactory({ document }));

    return true;
  }

  return false;
}

void UpdateSdl() {
  SDL_Event sdlEvent;

  auto& inputState = InputState::get();

  while (SDL_PollEvent(&sdlEvent)) {
    switch (sdlEvent.type) {
      case SDL_KEYDOWN:
        if (sdlEvent.key.keysym.sym < InputState::kMaxKey) {
          inputState.keyDown[sdlEvent.key.keysym.sym] = 1;
        }
        break;
      case SDL_KEYUP:
        if (sdlEvent.key.keysym.sym < InputState::kMaxKey) {
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

void MainLoop() {
  UpdateSdl();

  auto& sequencer = Sequencer::Get();
  if (sequencer.GetInstrument()) {
    EnableMenuItem(hMenu, ID_FILE_SAVEINSTRUMENT, MF_ENABLED);
    EnableMenuItem(hMenu, ID_FILE_NEWSONG, MF_ENABLED);
    EnableMenuItem(hMenu, ID_FILE_SAVESONG, MF_ENABLED);
  }
  else {
    EnableMenuItem(hMenu, ID_FILE_SAVEINSTRUMENT, MF_DISABLED);
    EnableMenuItem(hMenu, ID_FILE_NEWSONG, MF_DISABLED);
    EnableMenuItem(hMenu, ID_FILE_SAVESONG, MF_DISABLED);
  }

  glClearColor(0.5f, 0.5f, 0.5f, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  // Update viewport
  int windowWidth;
  int windowHeight;
  SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight);
  glViewport(0, 0, static_cast<GLuint>(windowWidth), static_cast<GLuint>(windowHeight));

  // Update matrices
  // Orthographic projection matrix
  {
    glm::mat4x4 screenOrtho(
      2.0f / windowWidth, 0.0f, 0.0f, 0.0f,
      0.0f, -2.0f / windowHeight, 0.0f, 0.0f,
      0.0f, 0.0f, -1.0f, 0.0f,
      -1.0f, 1.0f, 0.0f, 1.0f
    );
    GlobalRenderData::get().setMatrix(GlobalRenderData::MatrixType::ScreenOrthographic, screenOrtho);
  }

  ImGuiIO& imGuiIo = ImGui::GetIO();

  // Update time
  currentTime = static_cast<double>(SDL_GetTicks()) / 1000.0;
  static double lastUpdateTime = 0.0;
  imGuiIo.DeltaTime = static_cast<float>(currentTime - lastUpdateTime);
  lastUpdateTime = currentTime;

  // Update mouse
  auto& inputState = InputState::get();
  imGuiIo.MousePos = ImVec2(static_cast<float>(inputState.mouseX), static_cast<float>(inputState.mouseY));
  imGuiIo.MouseDown[0] = inputState.downL;
  imGuiIo.MouseDown[1] = false;
  imGuiIo.MouseWheel += static_cast<float>(inputState.scroll) * 0.5f;

  if (inputState.inputText.length()) {
    imGuiIo.AddInputCharactersUTF8(inputState.inputText.c_str());
    inputState.inputText.clear();
  }
  inputState.scroll = 0;

  for (auto k = 0; k < InputState::kMaxKey; ++k) {
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

  /////////////////////////////////////////////////////////////////////
  // RENDERING
  /////////////////////////////////////////////////////////////////////
  currentView->Render(currentTime,
    ImVec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight)));

  SDL_GL_SwapWindow(sdlWindow);
}

bool InitImGui() {
  ImGui::CreateContext();

  auto& imGuiIo = ImGui::GetIO();

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

  return true;
}

void TermImGui() {
  ImGui::DestroyContext();
}

bool InitGL() {
  glViewport(0, 0, kWindowWidth, kWindowHeight);
  glewInit();

  // Load shader programs
  GlobalRenderData::get().addShaderProgram(std::string("SpriteProgram"),
    ShaderProgram(std::string("SpriteProgram"),
      std::string("Shaders\\proj_pos2_scale_uv_diffuse.vert"),
      std::string("Shaders\\diffuse_mul_uv.frag"),
      // Automatic uniforms
      {
        { "ProjMtx",
          ShaderProgram::UniformInfo::Type::Matrix4x4,
          []() {
            return reinterpret_cast<const GLvoid*>(&GlobalRenderData::get().
              getMatrix(GlobalRenderData::MatrixType::ScreenOrthographic));
          }
        },
      })
  );

  GlobalRenderData::get().addShaderProgram(std::string("ImGuiProgram"),
    ShaderProgram(std::string("ImGuiProgram"),
      std::string("Shaders\\proj_pos2_uv_diffuse.vert"),
      std::string("Shaders\\diffuse_mul_uv.frag"),
      // Automatic uniforms
      {
        { "ProjMtx",
          ShaderProgram::UniformInfo::Type::Matrix4x4,
          []() {
            return reinterpret_cast<const GLvoid*>(&GlobalRenderData::get().
              getMatrix(GlobalRenderData::MatrixType::ScreenOrthographic));
          }
        },
      })
  );

  return true;
}

void TermGL() {

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
  ofn.lpstrFilter = _TEXT("JSON\0*.json\0");
  ofn.nFilterIndex = 0;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileName(&ofn)) {
    if (Sequencer::Get().LoadInstrument(std::string(W2A(szFile)), instrumentName)) {
      EnableMenuItem(hMenu, reinterpret_cast<UINT>(GetSubMenu(hMenu, 1)), MF_ENABLED);
      DrawMenuBar(sysWmInfo.info.win.window);
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

LRESULT CALLBACK MyWindowProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam) {
  switch (uMsg) {
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case ID_FILE_EXIT:
          wantQuit = true;
          return 0;
        case ID_FILE_NEWINSTRUMENT: {
          if (Sequencer::Get().NewInstrument()) {
            EnableMenuItem(hMenu, reinterpret_cast<UINT>(GetSubMenu(hMenu, 1)), MF_ENABLED);
            DrawMenuBar(sysWmInfo.info.win.window);
          }
          return 0;
        }
        case ID_ACCELERATOR_LOAD_INSTRUMENT:
        case ID_FILE_LOADINSTRUMENT: {
          LoadInstrument({});
          return 0;
        }
        case ID_FILE_SAVEINSTRUMENT: {
          SaveInstrument();
          return 0;
        }
        case ID_ACCELERATOR_LOAD_SONG:
        case ID_FILE_LOADSONG: {
          //if (sequencer.GetInstrument() != nullptr) 
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
              Sequencer::Get().LoadSong(std::string(W2A(szFile)));
            }
          }
          return 0;
        }
        case ID_ACCELERATOR_SAVE_SONG:
        case ID_FILE_SAVESONG: {
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
          return 0;
        }
        case ID_INSTRUMENT_ADDTRACK: {
          if (Sequencer::Get().GetInstrument() != nullptr) {
            if (AddSynthVoiceDialog()) {

            }
          }
          return 0;
        }
      }
  }
  return oldWindowProc(hWnd, uMsg, wParam, lParam);
}

bool Init() {
  Logging::AddResponder([](const std::string_view& logLine) {
    OutputDebugString(StringToWChar(logLine).get());
  });

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
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
    hMenu = LoadMenu(nullptr, MAKEINTRESOURCE(IDR_MENU_FILEINSTRUMENT));
    SetMenu(sysWmInfo.info.win.window, hMenu);
  }

  hMainWindowIcon = LoadIcon(sysWmInfo.info.win.hinstance, MAKEINTRESOURCE(IDI_MAINWINDOW2));
  SendMessage(sysWmInfo.info.win.window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hMainWindowIcon));
  SendMessage(sysWmInfo.info.win.window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hMainWindowIcon));

  hAccel = LoadAccelerators(sysWmInfo.info.
    win.hinstance, MAKEINTRESOURCE(IDR_ACCELERATOR_FILEMENU));
  SDL_SetWindowsMessageHook([](void *payload, void *hWnd, unsigned int message, Uint64 wParam, Sint64 lParam) {
    MSG msg = { 0 };
    msg.hwnd = reinterpret_cast<HWND>(hWnd);
    msg.lParam = static_cast<LPARAM>(lParam);
    msg.message = message;
    msg.wParam = static_cast<WPARAM>(wParam);
    TranslateAccelerator(sysWmInfo.info.win.window, hAccel, &msg);
  }, nullptr);

  oldWindowProc = reinterpret_cast<WNDPROC>(GetWindowLong(sysWmInfo.info.win.window, GWL_WNDPROC));
  SetWindowLong(sysWmInfo.info.win.window, GWL_WNDPROC, reinterpret_cast<LONG>(MyWindowProc));

  SDL_GL_CreateContext(sdlWindow);
  SDL_GL_SetSwapInterval(kSwapInterval);

  InitGL();
  InitImGui();

  // Initialize mixer
  if (!Mixer::InitSingleton(kAudioBufferSize)) {
    MCLOG(Error, "Unable to init Mixer.");
    SDL_Quit();
    return false;
  }

  // Initialize sequencer
  if (!Sequencer::InitSingleton(kDefaultNumMeasures, kDefaultBeatsPerMeasure,
    kDefaultBpm, TimelineDivisions.back(), kDefaultSubdivisions)) {
    MCLOG(Error, "Unable to initialize mixer");
    SDL_Quit();
    return false;
  }

  // TODO: determine default view
  Mixer::Get().SetController(&Sequencer::Get());

  Sequencer::Get().SetLoadInstrumentCallback(
    [](std::string instrumentName) {
      return LoadInstrument(instrumentName);
  });
  Sequencer::Get().SetMidiConversionParamsCallback(
    [](const MidiSource& midiSource, Sequencer::MidiConversionParams& midiConversionParams) {
      return GetMidiConversionParams(midiSource, midiConversionParams);
  });

  // Initialize the designer view
  currentView = new ComposerView;

  MCLOG(Info, "SynthCity %s", kVersionString);

  return true;
}

void Term() {
  // Term the sequencer
  Sequencer::TermSingleton();

  // Term the mixer
  Mixer::TermSingleton();

  // Term all views
  delete currentView;
  currentView = nullptr;

  TermImGui();
  TermGL();

  // Clean up Windows resources
  DestroyIcon(hMainWindowIcon);
  hMainWindowIcon = 0;
  DestroyAcceleratorTable(hAccel);
  hAccel = 0;
  DestroyMenu(hMenu);
  hMenu = 0;

  SDL_Quit();
}

int main(int argc, char **argv) {
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // tells leak detector to dump report at any program exit
  //_CrtSetBreakAlloc(262);

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
