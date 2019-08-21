#include "SDL.h"
#include "GL/glew.h"
#include "imgui.h"
#include <iostream>
#include <vector>
#include <chrono>
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
#include "ComposerView.h"
#include "GamePreviewView.h"
#include "InputState.h"
#include "Mixer.h"
#include "Globals.h"
#include "WavBank.h"
#include "Process.h"
#include "SoundFactory.h"
#include "ProcessFactory.h"


#include "SDL_syswm.h"
#include <windows.h>
#include <windowsx.h> // for GET_X_LPARAM and GET_Y_LPARAM
#include <atlbase.h>
// Stupid Windows
#undef max
#undef min
#include <fstream>

static SDL_Window* sdlWindow = nullptr;

static constexpr uint32 kDefaultNumMeasures = 2;
static constexpr uint32 kDefaultBeatsPerMeasure = 4;
static constexpr uint32 kDefaultSubdivisions = 4;
static constexpr uint32 kDefaultBpm = 120;
static constexpr uint32 kMaxSubdivisions = 8;
static constexpr int kAudioBufferSize = 2048;

static bool wantQuit = false;
static SDL_SysWMinfo sysWmInfo;

static constexpr uint32 kWindowWidth = 1200;
static constexpr uint32 kWindowHeight = 800;
static constexpr uint32 kSwapInterval = 1;

static GLuint fontTextureId;

InputState::MouseButton SdlMouseButtonToInputMouseButton(uint8 sdlMouseButton) {
  switch (sdlMouseButton) {
    case SDL_BUTTON_LEFT:
      return InputState::MouseButton::Left;
    case SDL_BUTTON_RIGHT:
      return InputState::MouseButton::Right;
    default:
      break;
  }
  return InputState::MouseButton::Count;
}

int32 SdlMouseButtonToInputMouseIndex(uint8 sdlMouseButton) {
  auto inputMouseButton = SdlMouseButtonToInputMouseButton(sdlMouseButton);
  if (inputMouseButton != InputState::MouseButton::Count) {
    return static_cast<int32>(inputMouseButton);
  }
  return -1;
}

void UpdateInput() {
  SDL_Event sdlEvent;

  auto& inputState = InputState::Get();

  inputState.BeginFrame();

  while (SDL_PollEvent(&sdlEvent)) {
    switch (sdlEvent.type) {
      case SDL_KEYDOWN:
        if (sdlEvent.key.keysym.sym < InputState::kMaxKey) {
          inputState.pressed[sdlEvent.key.keysym.sym] = true;
          inputState.keyDown[sdlEvent.key.keysym.sym] = true;
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
      case SDL_MOUSEBUTTONDOWN: {
        auto buttonIndex = SdlMouseButtonToInputMouseIndex(sdlEvent.button.button);
        if (buttonIndex != -1) {
          inputState.mouseButtonDown[buttonIndex] = true;
          inputState.mouseButtonPress[buttonIndex] = true;
        }
        break;
      }
      case SDL_MOUSEBUTTONUP: {
        auto buttonIndex = SdlMouseButtonToInputMouseIndex(sdlEvent.button.button);
        if (buttonIndex != -1) {
          inputState.mouseButtonDown[buttonIndex] = false;
          inputState.mouseButtonRelease[buttonIndex] = true;
        }
        break;
      }
      case SDL_MOUSEWHEEL:
        if (sdlEvent.wheel.y > 0) {
          inputState.mouseScrollSign = 1;
        }
        else if (sdlEvent.wheel.y < 0) {
          inputState.mouseScrollSign = -1;
        }
        break;
      case SDL_QUIT:
        wantQuit = true;
        break;
      default: {
        break;
      }
    }
  }

  inputState.modState = SDL_GetModState();

  ImGuiIO& imGuiIo = ImGui::GetIO();

  // Update time
  Globals::currentTime = static_cast<double>(SDL_GetTicks()) / 1000.0;
  static double lastUpdateTime = 0.0;
  imGuiIo.DeltaTime = std::max(1.0f / 120.0f, static_cast<float>(Globals::currentTime - lastUpdateTime));
  lastUpdateTime = Globals::currentTime;

  // Update mouse
  imGuiIo.MousePos = ImVec2(static_cast<float>(inputState.mouseX), static_cast<float>(inputState.mouseY));
  imGuiIo.MouseDown[0] = inputState.mouseButtonDown[InputState::MouseButton::Left];
  imGuiIo.MouseDown[1] = inputState.mouseButtonDown[InputState::MouseButton::Right];
  imGuiIo.MouseWheel += static_cast<float>(inputState.mouseScrollSign) * 0.5f;

  if (inputState.inputText.length()) {
    imGuiIo.AddInputCharactersUTF8(inputState.inputText.c_str());
  }

  // Update ImGui keys and record key press events
  for (auto k = 0; k < InputState::kMaxKey; ++k) {
    imGuiIo.KeysDown[k] = inputState.keyDown[k] != 0;
  }

  imGuiIo.KeyShift = (SDL_GetModState() & KMOD_SHIFT) != 0;
  imGuiIo.KeyCtrl = (SDL_GetModState() & KMOD_CTRL) != 0;
  imGuiIo.KeyAlt = (SDL_GetModState() & KMOD_ALT) != 0;
}

void MainLoop() {
  UpdateInput();

  auto& sequencer = Sequencer::Get();

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

  View::GetCurrentView()->Render(ImVec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight)));

  SDL_GL_SwapWindow(sdlWindow);
}

bool InitImGui() {
  ImGui::CreateContext();

  auto& imGuiIo = ImGui::GetIO();

  // Backup GL state
  GLint lastTexture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);

  // Create GL texture for font
  glGenTextures(1, &fontTextureId);
  glBindTexture(GL_TEXTURE_2D, fontTextureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  uchar* pixels;
  int width, height;
  imGuiIo.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  imGuiIo.Fonts->TexID = reinterpret_cast<ImTextureID>(fontTextureId);
  imGuiIo.Fonts->ClearInputData();
  imGuiIo.Fonts->ClearTexData();

  // Restore original state
  glBindTexture(GL_TEXTURE_2D, lastTexture);

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
      std::string("Shaders\\proj_pos2_scale_diffuse.vert"),
      std::string("Shaders\\diffuse.frag"),
      // Automatic uniforms
      {
        // Vertex shader
        { "ProjMtx",
          0,
          [](int32 location) {
            glUniformMatrix4fv(location, 1, GL_FALSE, 
              reinterpret_cast<const GLfloat*>(&GlobalRenderData::get().
                  getMatrix(GlobalRenderData::MatrixType::ScreenOrthographic)));
          }
        },
        {
          "WorldScale",
          1,
        },
      },
      // Attributes
      {
        { "Position", 0 },
        { "Color", 1 },
      })
  );

  GlobalRenderData::get().addShaderProgram(std::string("ImGuiProgram"),
    ShaderProgram(std::string("ImGuiProgram"),
      std::string("Shaders\\proj_pos2_uv_diffuse.vert"),
      std::string("Shaders\\diffuse_mul_uv.frag"),
      // Automatic uniforms
      {
        // Vertex shader
        { "ProjMtx",
          0,
          [](int32 location) {
            glUniformMatrix4fv(location, 1, GL_FALSE, 
              reinterpret_cast<const GLfloat*>(&GlobalRenderData::get().
                  getMatrix(GlobalRenderData::MatrixType::ScreenOrthographic)));
          }
        },
        // Fragment shader
        {
          "Texture",
          0,
        },
      },
      // Attributes
      {
        { "Position", 0 },
        { "UV", 1 },
        { "Color", 2 },
      })
  );

  return true;
}

void TermGL() {

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
  SDL_GetWindowWMInfo(sdlWindow, &sysWmInfo);

  SDL_GL_CreateContext(sdlWindow);
  SDL_GL_SetSwapInterval(kSwapInterval);

  InitGL();
  InitImGui();

  // Initialize WAV bank
  if (!WavBank::InitSingleton()) {
    MCLOG(Error, "Unable to init WAV bank");
    return false;
  }

  // Initialize mixer before sequencer, as sequencer will interact with mixer on load
  if (!Mixer::InitSingleton(kAudioBufferSize)) {
    MCLOG(Error, "Unable to init Mixer.");
    SDL_Quit();
    return false;
  }

  // Initialize sequencer
  if (!Sequencer::InitSingleton(kDefaultNumMeasures, kDefaultBeatsPerMeasure,
    kDefaultBpm, kMaxSubdivisions, kDefaultSubdivisions)) {
    MCLOG(Error, "Unable to initialize mixer");
    SDL_Quit();
    return false;
  }

  Mixer::Get().SetController(&Sequencer::Get());

  // Initialize the views
  View::RegisterView<ComposerView>(new ComposerView(reinterpret_cast<uint32>(sysWmInfo.info.win.window)));
  View::RegisterView<GamePreviewView>(new GamePreviewView(reinterpret_cast<uint32>(sysWmInfo.info.win.window)));
  View::SetCurrentView<ComposerView>();

  Sequencer::Get().LoadInstrument("Instrument\\808\\808.json", "");
  Sequencer::Get().LoadSongJson("Songs\\tapsimple.json");

  MCLOG(Info, "SynthCity %s", Globals::kVersionString);

  return true;
}

void Term() {
  // Term the mixer first, as it has to clean up sound instances which can refer to
  // sounds that are being kept alive by the sequencer/instrument
  Mixer::TermSingleton();

  // Term the sequencer
  Sequencer::TermSingleton();

  // Term the WAV bank
  WavBank::TermSingleton();

  // Term all views
  View::Term();

  TermImGui();
  TermGL();

  SDL_Quit();
}

int main(int argc, char **argv) {
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF); // tells leak detector to dump report at any program exit
  //_CrtSetBreakAlloc(1600);

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
