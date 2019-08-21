#include "GamePreviewView.h"
#include "SpriteRenderable.h"
#include "InputState.h"
#include "GL/glew.h"
#include "SDL.h"
#include "ComposerView.h"
#include "Sequencer.h"
#include "Instrument.h"

static glm::vec2 fretLineExtents(10, 750);
static glm::vec4 fretLineColor(0.2f, 0.2f, 0.2f, 1.0f);
static glm::vec2 fretLinePosition[] = {
  {  80, 20 },
  { 160, 20 },
  { 240, 20 },
  { 320, 20 }
};

static glm::vec2 fretNoteExtent(80.0f, 30.0f);
static glm::vec4 fretNoteColors[] = {
  glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
  glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
  glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
  glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
};

static glm::vec2 targetLinePosition(40, 680);
static glm::vec2 targetLineExtents(330, 10);

static constexpr uint32 kFretNoteFreListDefaultSize(60);
static uint32 kIntroBeats = 0;
static constexpr float kDistanceBetweenQuarterNotes = 200.0f;
static constexpr float kFretOffscreenMax = 100.0f;

class FretNote : public SpriteRenderable {
protected:
  uint32 fretIndex = UINT32_MAX;
public:
  FretNote() {

  }

  FretNote(uint32 fretIndex)
  : SpriteRenderable(fretNoteExtent, fretNoteColors[fretIndex])
  , fretIndex(fretIndex) {

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

  auto instrument = Sequencer::Get().GetInstrument();
  if (instrument != nullptr) {
    // Pre-spawn any necessary notes

    float subdivStep = kDistanceBetweenQuarterNotes / Sequencer::Get().GetMaxSubdivisions();

    // Start at the target line minus intro beats distance
    uint32 beatIndex = 0;
    float offsetY = targetLinePosition.y - kIntroBeats * kDistanceBetweenQuarterNotes;
    while (offsetY > -kFretOffscreenMax) {
      for (size_t trackIndex = 0; trackIndex < instrument->GetTracks().size(); ++trackIndex) {
        const auto& track = instrument->GetTrack(trackIndex);
        if (beatIndex < track->GetNoteCount()) {
          const auto& note = track->GetNote(beatIndex);
          if (note.enabled && note.fretIndex != -1) {
            auto fretNote = fretNoteFreeList.Borrow(note.fretIndex);
            fretNote->position = glm::vec2(fretLinePosition[note.
              fretIndex].x - fretNoteExtent.x * 0.5f, offsetY - fretNoteExtent.y);
            dynamicSprites.push_back(fretNote);
          }
        }
      }
      ++beatIndex;
      offsetY -= subdivStep;
    }

    //Sequencer::Get().Play();
  }
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

  for (const auto& renderable : dynamicSprites) {
    renderable->Render();
  }
}