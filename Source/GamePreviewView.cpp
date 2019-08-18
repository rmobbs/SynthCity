#include "GamePreviewView.h"
#include "SpriteRenderable.h"
#include "InputState.h"
#include "GL/glew.h"
#include "SDL.h"
#include "ComposerView.h"

static glm::vec2 fretLineExtents(10, 750);
static glm::vec4 fretLineColor(0.2f, 0.2f, 0.2f, 1.0f);
static glm::vec2 fretLinePosition[] = {
  {  80, 20 },
  { 160, 20 },
  { 240, 20 },
  { 320, 20 }
};

static glm::vec4 fretNoteColors[] = {
  glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
  glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
  glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
  glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
};

static glm::vec2 targetLinePosition(40, 680);
static glm::vec2 targetLineExtents(330, 10);

static constexpr uint32 kFretNoteFreListDefaultSize(60);

class FretNote {
protected:
  uint32 fretIndex = UINT32_MAX;
public:
  FretNote() {

  }
  FretNote(uint32 fretIndex)
  : fretIndex(fretIndex) {

  }
};

FreeList<FretNote, uint32> fretNoteFreeList;

GamePreviewView::GamePreviewView(uint32 mainWindowHandle)
  : mainWindowHandle(mainWindowHandle) {
  InitResources();
}

GamePreviewView::~GamePreviewView() {
  TermResources();
}

void GamePreviewView::InitResources() {
  // Fret lines
  for (int i = 0; i < _countof(fretLinePosition); ++i) {
    auto spriteRenderable = new SpriteRenderable(fretLineExtents, fretLineColor);
    spriteRenderable->position = fretLinePosition[i];
    staticSprites.push_back(spriteRenderable);
  }

  // Target line
  {
    auto spriteRenderable = new SpriteRenderable(targetLineExtents, fretLineColor);
    spriteRenderable->position = targetLinePosition;
    staticSprites.push_back(spriteRenderable);
  }

  // Note pools
  fretNoteFreeList.Init(kFretNoteFreListDefaultSize);
}

void GamePreviewView::TermResources() {
  for (const auto& renderable : staticSprites) {
    delete renderable;
  }


}

void GamePreviewView::Show() {
  // TODO: Read the settings

  // TODO: Menu

  // Instrument has a song loaded?

}

void GamePreviewView::HandleInput() {
  auto& inputState = InputState::Get();

  if (inputState.pressed[SDLK_ESCAPE]) {
    View::SetCurrentView<ComposerView>();
  }
}

void GamePreviewView::Render(ImVec2 canvasSize) {
  HandleInput();

  glClearColor(0.2f, 0.2f, 0.7f, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  for (const auto& renderable : staticSprites) {
    renderable->Render();
  }
}