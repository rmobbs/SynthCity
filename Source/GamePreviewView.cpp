#include "GamePreviewView.h"
#include "SpriteRenderable.h"
#include "InputState.h"
#include "GL/glew.h"
#include "SDL.h"
#include "ComposerView.h"
#include "Sequencer.h"
#include "Logging.h"
#include "Song.h"
#include "AudioGlobals.h"
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

static glm::vec2 fallingNoteExtent(72.0f, 24.0f);
static glm::vec4 fallingNoteColors[] = {
  glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
  glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
  glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
  glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
};

static constexpr uint32 kNumFretsPastTargetLine(1);

static glm::vec2 targetLinePosition(40, 680);
static glm::vec2 targetLineExtents(330, 10);

static constexpr uint32 kFretNoteFreListDefaultSize(60);
static constexpr float kDistanceBetweenQuarterNotes = 250.0f;
static constexpr float kFretOffscreenMax = 100.0f;
static constexpr uint32 kReadyBeats = 4;
static constexpr uint32 kCountdownBeats = 8;
static glm::vec4 kColorWhite(1.0f, 1.0f, 1.0f, 1.0f);
static glm::vec4 kColorTargetZone(1.0f, 1.0f, 0.0f, 0.5f);
static constexpr float kNoteEffectTimeInBeats = 0.25f;
static constexpr float kNoteBurstScale = 0.25f;

static glm::vec2 placardLocations[] = {
  { 600.0f, 200.0f },
  { 800.0f, 200.0f },
  { 600.0f, 400.0f },
  { 800.0f, 400.0f },
};
static glm::vec2 readyPlacardLocation(600.0f, 200.0f);

class NoteSprite : public SpriteRenderable {
protected:
  uint32 lineIndex = UINT32_MAX;
  float beatTime = 0.0f;
  bool triggered = false;
  float fadeEnd = 0.0f;
  bool doFade = false;
  bool canTrigger = true;
  glm::vec2 originalExtents;
  glm::vec2 triggerPosition;
public:
  NoteSprite() {

  }

  NoteSprite(float beatTime, uint32 lineIndex)
  : SpriteRenderable(fallingNoteExtent, fallingNoteColors[lineIndex])
  , beatTime(beatTime)
  , lineIndex(lineIndex)
  , originalExtents(fallingNoteExtent) {
 
  }

  inline float GetBeatTime() const {
    return beatTime;
  }
  inline uint32 GetGameLineIndex() const {
    return lineIndex;
  }

  inline bool Triggered() const {
    return triggered;
  }

  bool UpdateEffects(float beatTime) {
    if (doFade) {
      auto s = (fadeEnd - beatTime) / kNoteEffectTimeInBeats;

      extents.x = originalExtents.x * (1.0f + (kNoteBurstScale * s));
      extents.y = originalExtents.y * (1.0f + (kNoteBurstScale * s));

      position.x = triggerPosition.x - (extents.x - originalExtents.x) * 0.5f;
      position.y = triggerPosition.y - (extents.y - originalExtents.y) * 0.5f;

      color.a = s;

      return s > 0.0f;
    }
    return true;
  }

  void Trigger(float beatTime, bool doFade) {
    triggered = true;
    this->doFade = doFade;
    fadeEnd = beatTime + kNoteEffectTimeInBeats;
    triggerPosition = position;
  }

  void Disqualify() {
    canTrigger = false;
  }

  bool Triggerable() {
    return canTrigger && !triggered;
  }
};

FreeList<NoteSprite, float, uint32> fallingNoteFreeList;

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
  targetWindowScale = fallingNoteExtent.y * 0.5f;

  targetWindowDeltaPos = targetWindowScale;
  targetWindowDeltaNeg = -(targetWindowScale + targetLineExtents.y + fallingNoteExtent.y);

  // TODO: Need texture management https://trello.com/c/BRVZcDuP
  uint32 w, h;

  // White texture
  whiteTextureId = LoadTexture("Assets\\white.png", &w, &h);
  whiteTextureSize.x = static_cast<float>(w);
  whiteTextureSize.y = static_cast<float>(h);

  // Zone renderer
  targetZone = new SpriteRenderable(glm::vec2(targetLineExtents.x, 
    targetWindowScale * 2.0f + targetLineExtents.y), kColorTargetZone);
  targetZone->AddTexture(whiteTextureId);
  targetZone->SetPosition(glm::vec2(targetLinePosition.x, targetLinePosition.y - targetWindowScale));

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
  assert(w == fallingNoteExtent.x);
  assert(h == fallingNoteExtent.y);
  fallingNoteFreeList.Init(kFretNoteFreListDefaultSize);
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

  delete targetZone;
  targetZone = nullptr;

  delete readyPlacard;
  readyPlacard = nullptr;
}

void GamePreviewView::OnBeat(uint32 beat) {

  switch (beat) {
    case 1:
      mode = Mode::Ready;
      break;
    case 5:
      mode = Mode::Countdown_12;
      Sequencer::Get().PlayMetronome(true);
      break;
    case 7:
      mode = Mode::Countdown_34;
      Sequencer::Get().PlayMetronome(true);
      break;
    case 9:
      mode = Mode::Countdown_1;
      Sequencer::Get().PlayMetronome(true);
      break;
    case 10:
      mode = Mode::Countdown_2;
      Sequencer::Get().PlayMetronome(true);
      break;
    case 11:
      mode = Mode::Countdown_3;
      Sequencer::Get().PlayMetronome(true);
      break;
    case 12:
      mode = Mode::Countdown_4;
      Sequencer::Get().PlayMetronome(true);
      break;
    case 13:
      mode = Mode::Playing;
      break;
    default:
      break;
  }
}

void GamePreviewView::Show() {
  auto& sequencer = Sequencer::Get();

  auto song = sequencer.GetSong();
  // Pre-spawn any necessary notes

  float subdivStep = kDistanceBetweenQuarterNotes / song->GetMinNoteValue();

  // Spawn initial notes
  float offset = static_cast<float>(kReadyBeats + kCountdownBeats);
  for (size_t noteIndex = 0; noteIndex < song->GetNoteCount(); ++noteIndex) {
    for (size_t lineIndex = 0; lineIndex < song->GetLineCount(); ++lineIndex) {
      const auto& note = song->GetLine(lineIndex)[noteIndex];
      if (note.GetEnabled() && note.GetGameIndex() != -1) {
        auto fallingNote = fallingNoteFreeList.Borrow(offset + static_cast<float>(noteIndex) /
          static_cast<float>(song->GetMinNoteValue()), note.GetGameIndex());
        fallingNote->SetExtents(fallingNoteExtent);
        fallingNote->AddTexture(fallingNoteTextureId);
        fallingNotes.push_back(fallingNote);
      }
    }
  }

  beatCallbackId = Sequencer::Get().AddBeatCallback(
    [](uint32 beat, bool isDownBeat, void* payload) {
      reinterpret_cast<GamePreviewView*>(payload)->OnBeat(beat);
    }, this);

  mode = Mode::Ready;

  sequencer.PrepareGameplay(4);
  sequencer.SetGameplayMode(true);
  sequencer.SetIntroBeats(kReadyBeats + kCountdownBeats);
  sequencer.SetLooping(false);
  sequencer.Play();

  perfectScore = fallingNotes.size() * 1.0f;
  currentScore = 0.0f;
  noteStreak = 0;
}

void GamePreviewView::Hide() {
  auto& sequencer = Sequencer::Get();

  sequencer.SetGameplayMode(false);

  fallingNotes.clear();

  if (beatCallbackId != UINT32_MAX) {
    sequencer.RemoveBeatCallback(beatCallbackId);
    beatCallbackId = UINT32_MAX;
  }
  sequencer.Stop();
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

  if (drawZone) {
    targetZone->Render();
  }

  for (const auto& renderable : staticSprites) {
    renderable->Render();
  }

  switch (mode) {
    case Mode::Ready: {
      readyPlacard->Render();
      break;
    }
    case Mode::Countdown_12: {
      countdownPlacards[0]->Render();
      break;
    }
    case Mode::Countdown_34: {
      countdownPlacards[1]->Render();
      break;
    }
    case Mode::Countdown_1: {
      countdownPlacards[0]->Render();
      break;
    }
    case Mode::Countdown_2: {
      countdownPlacards[1]->Render();
      break;
    }
    case Mode::Countdown_3: {
      countdownPlacards[2]->Render();
      break;
    }
    case Mode::Countdown_4: {
      countdownPlacards[3]->Render();
      break;
    }
    default:
      break;
  }

  // Get current high-precision beat time
  float beatTime = Sequencer::Get().GetBeatTime();
  float beatFrac = beatTime - std::floorf(beatTime);

  // Position rolling frets so their top is touching the target line as the beat falls;
  // that way notes rest on top of the frets
  float bottomY = targetLinePosition.y + kDistanceBetweenQuarterNotes * kNumFretsPastTargetLine;

  // Barber pole illusion for frets
  for (uint32 i = 0; i < fretSprites.size(); ++i) {
    const auto& renderable = fretSprites[i];
    renderable->position = glm::vec2(noteLanePosition[0].x,
      bottomY - ((i + (1.0f - beatFrac)) * kDistanceBetweenQuarterNotes));
    renderable->Render();
  }

  // Check for button presses
  std::array<float, GameGlobals::kNumGameplayLines> presses;
  Sequencer::Get().GetGameInput().ConsumePresses(presses);

  // Update falling notes
  auto fallingNoteIter = fallingNotes.begin();
  while (fallingNoteIter != fallingNotes.end()) {
    auto fallingNote = *fallingNoteIter;

    if (fallingNote->Triggerable()) {
      auto lineIndex = fallingNote->GetGameLineIndex();
      if (presses[lineIndex] > 0.0f) {
        auto visualDelta = (fallingNote->GetBeatTime() - presses[lineIndex]) * kDistanceBetweenQuarterNotes;

        if (visualDelta < targetWindowScale) {
          if (visualDelta > targetWindowDeltaNeg) {
            // Trigger it
            fallingNote->Trigger(beatTime, true);

            // Position (UL-relative)
            fallingNote->position.y = targetLinePosition.y - fallingNoteExtent.y - visualDelta;

            // Update score
            float score = 0.0f;
            if (std::fabsf(visualDelta) < std::numeric_limits<float>::epsilon()) {
              score += 1.0f;
            }
            else if (visualDelta > 0.0f) {
              score += 1.0f - (visualDelta / targetWindowScale);
            }
            else {
              score += 1.0f - (visualDelta / targetWindowDeltaNeg);
            }

            currentScore += score;
            streakScore += score;

            // Update note streak
            ++noteStreak;
          }
          else {
            // Disqualify it
            fallingNote->Disqualify();

            // Reset note streak
            noteStreak = 0;
            streakScore = 0.0f;
          }
        }
      }
    }

    // If it's already triggered, update its fade out effects until it expires
    if (fallingNote->Triggered()) {
      if (!fallingNote->UpdateEffects(beatTime)) {
        fallingNoteIter = fallingNotes.erase(fallingNoteIter);
        fallingNoteFreeList.Return(fallingNote);
        continue;
      }
    }
    // Otherwise keep dropping it
    else {
      float notePosY = targetLinePosition.y - fallingNoteExtent.y -
        (fallingNote->GetBeatTime() - beatTime) * kDistanceBetweenQuarterNotes;

      // Bottom of screen
      if (notePosY >= canvasSize.y) {
        noteStreak = 0;
        streakScore = 0.0f;
        fallingNoteIter = fallingNotes.erase(fallingNoteIter);
        fallingNoteFreeList.Return(fallingNote);
        continue;
      }
      // Top of screen
      else if (notePosY < -fretExtents.y) {
        break;
      }
      // The rest
      else {
        fallingNote->position = glm::vec2(noteLanePosition[fallingNote->
          GetGameLineIndex()].x + (noteLaneExtents.x * 0.5f) - fallingNoteExtent.x * 0.5f, notePosY);
      }
    }

    fallingNote->Render();
    ++fallingNoteIter;
  }



  ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(canvasSize.x), static_cast<float>(canvasSize.y));
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(canvasSize.x - 180.0f, 10.0f));
  ImGui::Begin("Instructions", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  {
    ImGui::Text("Press ESC to exit");
    ImGui::Checkbox("Draw zone", &drawZone);

    for (uint32 lineIndex = 0; lineIndex < GameGlobals::kNumGameplayLines; ++lineIndex) {
      std::string label("Key " + std::to_string(lineIndex + 1));
      if (ImGui::Button(label.c_str())) {
        mappingKey = lineIndex;
      }
      ImGui::SameLine();
      ImGui::Text(SDL_GetKeyName(static_cast<SDL_Keycode>(Sequencer::Get().GetGameInput().GetLineKey(lineIndex))));
    }

    if (mappingKey != -1) {

      ImGui::Text("Press any key ...");

      auto keyCode = InputState::GetFirstPressedKey();
      if (keyCode != -1) {
        AudioGlobals::LockAudio();
        Sequencer::Get().GetGameInput().SetLineKey(mappingKey, keyCode);
        AudioGlobals::UnlockAudio();
        mappingKey = -1;
      }
    }

    ImGui::Separator();
    char scoreBuf[256];
    sprintf(scoreBuf, "Score: %03.2f/%03.2f", currentScore, perfectScore);
    ImGui::Text(scoreBuf);
    std::string streakString("Streak: " + std::to_string(noteStreak));
    ImGui::Text(streakString.c_str());
    sprintf(scoreBuf, "Streak score: %03.2f/%03.2f", streakScore, static_cast<float>(noteStreak));
    ImGui::Text(scoreBuf);

    ImGui::End();
  }

  renderable.Render();
}