#include "GamePreviewView.h"
#include "SpriteRenderable.h"
#include "InputState.h"
#include "GL/glew.h"
#include "SDL.h"
#include "ComposerView.h"
#include "Sequencer.h"
#include "Song.h"
#include "soil.h"

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
static constexpr uint32 kReadyBeats = 4;
static constexpr uint32 kCountdownBeats = 8;
static glm::vec4 kColorWhite(1.0f, 1.0f, 1.0f, 1.0f);

static glm::vec2 placardLocations[] = {
  { 600.0f, 200.0f },
  { 800.0f, 200.0f },
  { 600.0f, 400.0f },
  { 800.0f, 400.0f },
};
static glm::vec2 readyPlacardLocation(600.0f, 200.0f);

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

uint32 GamePreviewView::LoadTexture(const std::string& textureName, uint32* outWidth, uint32* outHeight) {
  int32 width, height;
  uint8* textureData = SOIL_load_image(textureName.c_str(), &width, &height, 0, SOIL_LOAD_RGBA);
  if (textureData) {
    uint32 textureId;
    int32 lastTextureId;

    glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTextureId);

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData);
    SOIL_free_image_data(textureData);

    glBindTexture(GL_TEXTURE_2D, lastTextureId);

    loadedTextures.push_back(textureId);

    if (outWidth != nullptr) {
      *outWidth = width;
    }
    if (outHeight != nullptr) {
      *outHeight = height;
    }
    return textureId;
  }
  return UINT32_MAX;
}

void GamePreviewView::InitResources() {
  // TODO: Need texture management https://trello.com/c/BRVZcDuP
  uint32 w, h;

  // White texture
  whiteTextureId = LoadTexture("Assets\\white.png", &w, &h);
  whiteTextureSize.x = static_cast<float>(w);
  whiteTextureSize.y = static_cast<float>(h);

  // Placards
  auto readyTexture = LoadTexture("Assets\\ReadyPlacard.png", &w, &h);
  readyPlacard = new SpriteRenderable(glm::vec2(w, h), kColorWhite);
  readyPlacard->AddTexture(readyTexture);
  readyPlacard->SetPosition(readyPlacardLocation);

  auto count1Texture = LoadTexture("Assets\\OnePlacard.png", &w, &h);
  countdownPlacards.push_back(new SpriteRenderable(glm::vec2(w, h), kColorWhite));
  countdownPlacards.back()->AddTexture(count1Texture);
  countdownPlacards.back()->SetPosition(placardLocations[0]);

  auto count2Texture = LoadTexture("Assets\\TwoPlacard.png", &w, &h);
  countdownPlacards.push_back(new SpriteRenderable(glm::vec2(w, h), kColorWhite));
  countdownPlacards.back()->AddTexture(count2Texture);
  countdownPlacards.back()->SetPosition(placardLocations[1]);

  auto count3Texture = LoadTexture("Assets\\ThreePlacard.png", &w, &h);
  countdownPlacards.push_back(new SpriteRenderable(glm::vec2(w, h), kColorWhite));
  countdownPlacards.back()->AddTexture(count3Texture);
  countdownPlacards.back()->SetPosition(placardLocations[2]);

  auto count4Texture = LoadTexture("Assets\\FourPlacard.png", &w, &h);
  countdownPlacards.push_back(new SpriteRenderable(glm::vec2(w, h), kColorWhite));
  countdownPlacards.back()->AddTexture(count4Texture);
  countdownPlacards.back()->SetPosition(placardLocations[3]);


  // Note lanes
  for (int i = 0; i < _countof(noteLanePosition); ++i) {
    auto spriteRenderable = new SpriteRenderable(noteLaneExtents, noteLaneColor);
    spriteRenderable->SetPosition(noteLanePosition[i]);
    spriteRenderable->AddTexture(whiteTextureId);
    staticSprites.push_back(spriteRenderable);
  }

  // Moving frets
  float bottomY = targetLinePosition.y + kDistanceBetweenQuarterNotes * kNumFretsPastTargetLine;
  for (uint32 i = 0; i < kNumFrets; ++i) {
    auto spriteRenderable = new SpriteRenderable(fretExtents, fretColor);
    spriteRenderable->SetPosition(glm::vec2(noteLanePosition[0].x, bottomY - i * kDistanceBetweenQuarterNotes));
    spriteRenderable->AddTexture(whiteTextureId);
    fretSprites.push_back(spriteRenderable);
  }

  // Target line
  {
    auto spriteRenderable = new SpriteRenderable(targetLineExtents, noteLaneColor);
    spriteRenderable->SetPosition(targetLinePosition);
    spriteRenderable->AddTexture(whiteTextureId);
    staticSprites.push_back(spriteRenderable);
  }

  // Falling notes
  fallingNoteTextureId = LoadTexture("Assets\\droprectangle.png", &w, &h);
  fallingNoteTextureSize.x = static_cast<float>(w);
  fallingNoteTextureSize.y = static_cast<float>(h);
  fretNoteFreeList.Init(kFretNoteFreListDefaultSize);
}

void GamePreviewView::TermResources() {
  for (const auto& renderable : staticSprites) {
    delete renderable;
  }
  staticSprites.clear();

  for (const auto& renderable : fretSprites) {
    delete renderable;
  }
  fretSprites.clear();

  for (const auto& countdownPlacard : countdownPlacards) {
    delete countdownPlacard;
  }
  countdownPlacards.clear();

  delete readyPlacard;
  readyPlacard = nullptr;
}

void GamePreviewView::OnBeat() {
  switch (mode) {
    case Mode::Ready: {
      --beatsLeftInState;
      if (!beatsLeftInState) {
        mode = Mode::Countdown;
        beatsLeftInState = kCountdownBeats;
      }
      break;
    }
    case Mode::Countdown: {
      --beatsLeftInState;
      if (!beatsLeftInState) {
        mode = Mode::Playing;
        Sequencer::Get().Play();
      }
      break;
    }
    default:
      break;
  }
}

void GamePreviewView::Show() {
  // Length of a full beat in frames
  beatTickLength = Mixer::kDefaultFrequency / Sequencer::Get().GetTempo() * 60.0f;

  auto song = Sequencer::Get().GetSong();
  // Pre-spawn any necessary notes

  float subdivStep = kDistanceBetweenQuarterNotes / song->GetMinNoteValue();

  // Spawn initial notes
  float offset = static_cast<float>(kReadyBeats + kCountdownBeats);
  for (size_t noteIndex = 0; noteIndex < song->GetNoteCount(); ++noteIndex) {
    for (size_t lineIndex = 0; lineIndex < song->GetLineCount(); ++lineIndex) {
      const auto& note = song->GetLine(lineIndex)[noteIndex];
      if (note.GetEnabled() && note.GetGameIndex() != -1) {
        auto fretNote = fretNoteFreeList.Borrow(offset + static_cast<float>(noteIndex) /
          static_cast<float>(song->GetMinNoteValue()), note.GetGameIndex());
        fretNote->SetExtents(fallingNoteTextureSize);
        fretNote->AddTexture(fallingNoteTextureId);
        fallingNotes.push_back(fretNote);
      }
    }
  }

  Sequencer::Get().SetLooping(false);
  Sequencer::Get().ResetFrameCounter();
  beatCallbackId = Sequencer::Get().AddBeatCallback(
    [](void* payload) {
      reinterpret_cast<GamePreviewView*>(payload)->OnBeat();
    }, this);

  mode = Mode::Ready;
  beatsLeftInState = kReadyBeats;
}

void GamePreviewView::Hide() {
  if (beatCallbackId != UINT32_MAX) {
    Sequencer::Get().RemoveBeatCallback(beatCallbackId);
    beatCallbackId = UINT32_MAX;
  }
  Sequencer::Get().Stop();
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

  switch (mode) {
    case Mode::Ready: {
      readyPlacard->Render();
      break;
    }
    case Mode::Countdown: {
      switch (beatsLeftInState) {
      case 8:
      case 7:
        countdownPlacards[0]->Render();
        break;
      case 6:
      case 5:
        countdownPlacards[1]->Render();
        break;
      case 4:
        countdownPlacards[0]->Render();
        break;
      case 3:
        countdownPlacards[1]->Render();
        break;
      case 2:
        countdownPlacards[2]->Render();
        break;
      case 1:
        countdownPlacards[3]->Render();
        break;
      }
      break;
    }
    default:
      break;
  }

  // Get current high-precision beat time
  float beatTime = mode != Mode::Ready ? static_cast<float>(Mixer::Get().GetCurTicks()) / beatTickLength : 0.0f;
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