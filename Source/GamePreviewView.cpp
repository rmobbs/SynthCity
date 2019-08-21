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
  float beatTime = 0.0f;
public:
  FretNote() {

  }

  FretNote(float beatTime, uint32 fretIndex)
  : SpriteRenderable(fretNoteExtent, fretNoteColors[fretIndex])
  , beatTime(beatTime)
  , fretIndex(fretIndex) {

  }

  inline float GetBeatTime() const {
    return beatTime;
  }
  inline uint32 GetFretIndex() const {
    return fretIndex;
  }
};

FreeList<FretNote, float, uint32> fretNoteFreeList;

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
  // Length of a full beat in frames
  beatFrameLength = Mixer::kDefaultFrequency / Sequencer::Get().GetBeatsPerMinute() * 60.0f;

  // TODO: Read the settings

  // TODO: Menu

  // Instrument has a song loaded?

  auto instrument = Sequencer::Get().GetInstrument();
  if (instrument != nullptr) {
    // Pre-spawn any necessary notes

    float subdivStep = kDistanceBetweenQuarterNotes / Sequencer::Get().GetMaxSubdivisions();

    // Spawn initial notes
    for (uint32 subBeatIndex = 0; subBeatIndex < 8 * Sequencer::Get().GetMaxSubdivisions(); ++subBeatIndex) {
      for (size_t trackIndex = 0; trackIndex < instrument->GetTracks().size(); ++trackIndex) {
        const auto& track = instrument->GetTrack(trackIndex);
        if (subBeatIndex < track->GetNoteCount()) {
          const auto& note = track->GetNote(subBeatIndex);
          if (note.enabled && note.fretIndex != -1) {
            auto fretNote = fretNoteFreeList.Borrow(static_cast<float>(subBeatIndex) /
              static_cast<float>(Sequencer::Get().GetMaxSubdivisions()), note.fretIndex);
            dynamicSprites.push_back(fretNote);
          }
        }
        else {
          break;
        }
      }
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

  // Update falling notes

  // Get current high-precision beat time
  float beatTime = static_cast<float>(Mixer::Get().GetCurFrame()) / beatFrameLength;

  // Roll through notes and update positions
  auto fretNoteIter = dynamicSprites.begin();
  while (fretNoteIter != dynamicSprites.end()) {
    auto fretNote = reinterpret_cast<FretNote*>(*fretNoteIter);

    float notePosY = targetLinePosition.y -
      (fretNote->GetBeatTime() - beatTime) * kDistanceBetweenQuarterNotes;
    if (notePosY >= targetLinePosition.y) {
      fretNoteIter = dynamicSprites.erase(fretNoteIter);
    }
    else if (notePosY < -100.0f) {
      break;
    }
    else {
      fretNote->position = glm::vec2(fretLinePosition[fretNote->
        GetFretIndex()].x - fretNoteExtent.x * 0.5f, notePosY);
      fretNote->Render();
      ++fretNoteIter;
    }
  }
}