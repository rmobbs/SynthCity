#include "GamePreviewView.h"
#include "SpriteRenderable.h"
#include "InputState.h"
#include "GL/glew.h"
#include "SDL.h"
#include "ComposerView.h"
#include "Sequencer.h"
#include "Song.h"

static glm::vec2 noteLaneExtents(10, 800);
static glm::vec4 noteLaneColor(0.2f, 0.2f, 0.2f, 1.0f);
static glm::vec2 noteLanePosition[] = {
  {  80, 0 },
  { 160, 0 },
  { 240, 0 },
  { 320, 0 }
};
static glm::vec2 fretExtents(250, 10);
static constexpr uint32 kNumFrets(5);
static glm::vec4 fretColor(0.2f, 0.2f, 0.2f, 1.0f);

static glm::vec2 fretNoteExtent(70.0f, 20.0f);
static glm::vec4 fretNoteColors[] = {
  glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
  glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
  glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
  glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
};

static constexpr uint32 kNumFretsPastTargetLine(1);

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
  // Note lanes
  for (int i = 0; i < _countof(noteLanePosition); ++i) {
    auto spriteRenderable = new SpriteRenderable(noteLaneExtents, noteLaneColor);
    spriteRenderable->position = noteLanePosition[i];
    staticSprites.push_back(spriteRenderable);
  }

  // Moving frets
  float bottomY = targetLinePosition.y + kDistanceBetweenQuarterNotes * kNumFretsPastTargetLine;
  for (uint32 i = 0; i < kNumFrets; ++i) {
    auto spriteRenderable = new SpriteRenderable(fretExtents, fretColor);
    spriteRenderable->position = glm::vec2(noteLanePosition[0].x, bottomY - i * kDistanceBetweenQuarterNotes);
    fretSprites.push_back(spriteRenderable);
  }

  // Target line
  {
    auto spriteRenderable = new SpriteRenderable(targetLineExtents, noteLaneColor);
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

  for (const auto& renderable : fretSprites) {
    delete renderable;
  }
}

void GamePreviewView::Show() {
  // Length of a full beat in frames
  beatFrameLength = Mixer::kDefaultFrequency / Sequencer::Get().GetTempo() * 60.0f;

  // TODO: Read the settings

  // TODO: Menu

  // Instrument has a song loaded?

  auto song = Sequencer::Get().GetSong();
  if (song != nullptr) {
    // Pre-spawn any necessary notes

    float subdivStep = kDistanceBetweenQuarterNotes / song->GetMinNoteValue();

    // Spawn initial notes
    for (size_t noteIndex = 0; noteIndex < song->GetNoteCount(); ++noteIndex) {
      for (size_t lineIndex = 0; lineIndex < song->GetLineCount(); ++lineIndex) {
        const auto& note = song->GetLine(lineIndex)[noteIndex];
        if (note.GetEnabled() && note.GetGameIndex() != -1) {
          auto fretNote = fretNoteFreeList.Borrow(static_cast<float>(noteIndex) /
            static_cast<float>(song->GetMinNoteValue()), note.GetGameIndex());
          fallingNotes.push_back(fretNote);
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

  // Get current high-precision beat time
  float beatTime = static_cast<float>(Mixer::Get().GetCurFrame()) / beatFrameLength;
  float beatFrac = beatTime - std::floorf(beatTime);

  float bottomY = targetLinePosition.y + kDistanceBetweenQuarterNotes * kNumFretsPastTargetLine;

  // Barber pole illusion for frets
  for (uint32 i = 0; i < fretSprites.size(); ++i) {
    const auto& renderable = fretSprites[i];
    renderable->position = glm::vec2(noteLanePosition[0].x,
      bottomY - ((i + (1.0f - beatFrac)) * kDistanceBetweenQuarterNotes));
    renderable->Render();
  }

  // Update falling notes
  auto fretNoteIter = fallingNotes.begin();
  while (fretNoteIter != fallingNotes.end()) {
    auto fretNote = reinterpret_cast<FretNote*>(*fretNoteIter);

    float notePosY = (targetLinePosition.y + (targetLineExtents.y * 0.5f)) - (fretNoteExtent.y * 0.5f) -
      (fretNote->GetBeatTime() - beatTime) * kDistanceBetweenQuarterNotes;
    if (notePosY >= targetLinePosition.y) {
      fretNoteIter = fallingNotes.erase(fretNoteIter);
    }
    else if (notePosY < -100.0f) {
      break;
    }
    else {
      fretNote->position = glm::vec2(noteLanePosition[fretNote->
        GetFretIndex()].x + (noteLaneExtents.x * 0.5f) - fretNoteExtent.x * 0.5f, notePosY);
      fretNote->Render();
      ++fretNoteIter;
    }
  }
}