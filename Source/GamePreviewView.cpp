#include "GamePreviewView.h"
#include "SpriteRenderable.h"
#include "InputState.h"
#include "GL/glew.h"
#include "SDL.h"
#include "ComposerView.h"
#include "Sequencer.h"
#include "Logging.h"
#include "Song.h"
#include "Instrument.h"
#include "AudioGlobals.h"
#include "soil.h"
#include <chrono>

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

static constexpr char kGameplayKeys[] = { 'a', 's', 'd', 'f' };

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
  Track* track = nullptr;
public:
  NoteSprite() {

  }

  NoteSprite(float beatTime, uint32 lineIndex)
  : SpriteRenderable(fallingNoteExtent, fallingNoteColors[lineIndex])
  , beatTime(beatTime)
  , lineIndex(lineIndex)
  , originalExtents(fallingNoteExtent) {
 
  }

  inline void SetTrack(Track* track) {
    this->track = track;
  }
  inline Track* GetTrack() const {
    return track;
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
  : mainWindowHandle(mainWindowHandle)
  , gameInput({ 'a', 's', 'd', 'f' }) {
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

  // TODO: Need texture management https://trello.com/c/BRVZcDuP
  uint32 w, h;

  // White texture
  whiteTextureId = LoadTexture("Assets\\white.png", &w, &h);
  whiteTextureSize.x = static_cast<float>(w);
  whiteTextureSize.y = static_cast<float>(h);

  // Zone renderer
  targetZone = new SpriteRenderable(glm::vec2(targetLineExtents.x, 
    targetWindowScale * 2.0f), kColorTargetZone);
  targetZone->AddTexture(whiteTextureId);
  targetZone->SetPosition(glm::vec2(targetLinePosition.x,
    (targetLinePosition.y + targetLineExtents.y * 0.5f) - targetWindowScale +
    (beatTimeSlop * kDistanceBetweenQuarterNotes)));

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

void GamePreviewView::OnBeat(uint32 beatIndex) {

  if (mode != Mode::Playing) {
    if ((beatIndex % Sequencer::Get().GetSong()->GetMinNoteValue()) == 0) {

      auto fullBeat = beatIndex / Sequencer::Get().GetSong()->GetMinNoteValue();
      switch (fullBeat) {
        case 0:
          mode = Mode::Ready;
          break;
        case 4:
          mode = Mode::Countdown_12;
          Sequencer::Get().PlayMetronome(true);
          break;
        case 6:
          mode = Mode::Countdown_34;
          Sequencer::Get().PlayMetronome(true);
          break;
        case 8:
          mode = Mode::Countdown_1;
          Sequencer::Get().PlayMetronome(true);
          break;
        case 9:
          mode = Mode::Countdown_2;
          Sequencer::Get().PlayMetronome(true);
          break;
        case 10:
          mode = Mode::Countdown_3;
          Sequencer::Get().PlayMetronome(true);
          break;
        case 11:
          mode = Mode::Countdown_4;
          Sequencer::Get().PlayMetronome(true);
          break;
        case 12:
          mode = Mode::Playing;
          break;
        default:
          break;
      }
    }
  }

  if (beatIndex >= introBeats) {
    auto& sequencer = Sequencer::Get();

    auto songBeat = beatIndex - introBeats;
    for (auto& autoLine : autoNotes) {
      if (autoLine.cur != autoLine.end) {
        if (autoLine.cur->GetBeatIndex() == songBeat) {
          // Autoplay any non-tagged notes
          if (autoLine.cur->GetGameIndex() == kInvalidUint32) {
            sequencer.PlayPatch(autoLine.track->GetPatch(), autoLine.track->GetVolume());
          }
          ++autoLine.cur;
        }
      }
    }
  }
}

void GamePreviewView::Show() {
  // Gotta catch 'em all
  auto& sequencer = Sequencer::Get();
  auto song = sequencer.GetSong();

  uint32 numFallingNotes = 0;

  const auto& instrumentInstances = song->GetInstrumentInstances();
  for (const auto& instrumentInstance : instrumentInstances) {
    // Pre-spawn and position note sprites on gameplay lines
    const auto& trackInstances = instrumentInstance->trackInstances;
    for (const auto& trackInstance : trackInstances) {
      for (const auto& note : trackInstance.second.noteList) {
        auto gameIndex = note.GetGameIndex();
        if (gameIndex < GameGlobals::kNumGameplayLines) {
          auto fallingNote = fallingNoteFreeList.Borrow(static_cast<float>(kReadyBeats + kCountdownBeats) +
            static_cast<float>(note.GetBeatIndex()) /
            static_cast<float>(song->GetMinNoteValue()), gameIndex);
          fallingNote->SetTrack(instrumentInstance->instrument->GetTrackById(trackInstance.first));
          fallingNote->SetExtents(fallingNoteExtent);
          fallingNote->AddTexture(fallingNoteTextureId);
          fallingNotes[gameIndex].push_back(fallingNote);
          ++numFallingNotes;
        }
      }
    }
  }

  // Want them sorted by beat time
  for (uint32 gameLine = 0; gameLine < GameGlobals::kNumGameplayLines; ++gameLine) {
    fallingNotes[gameLine].sort([](const NoteSprite* a, const NoteSprite* b) {
      return a->GetBeatTime() < b->GetBeatTime();
    });
  }

  // Setup our autoplay
  for (const auto& instrumentInstance : instrumentInstances) {
    const auto& trackInstances = instrumentInstance->trackInstances;
    for (const auto& trackInstance : trackInstances) {
      autoNotes.push_back(
        { instrumentInstance->instrument->GetTrackById(trackInstance.first),
          trackInstance.second.noteList.begin(),
          trackInstance.second.noteList.end()
        });
    }
  }

  // Used in the beat callback, which is min beat
  introBeats = (kReadyBeats + kCountdownBeats) * song->GetMinNoteValue();
  perfectScore = numFallingNotes * 1.0f;
  currentScore = 0.0f;
  noteStreak = 0;
  mode = Mode::Ready;

  sequencer.Play();
}

void GamePreviewView::Hide() {
  autoNotes.clear();

  for (uint32 gameLine = 0; gameLine < GameGlobals::kNumGameplayLines; ++gameLine) {
    fallingNotes[gameLine].clear();
    lineTracks[gameLine] = nullptr;
  }
  fallingNoteFreeList.ReturnAll();

  Sequencer::Get().StopKill();
}

void GamePreviewView::HandleInput() {
  auto& inputState = InputState::Get();

  if (inputState.pressed[SDLK_ESCAPE]) {
    View::SetCurrentView<ComposerView>();
  }
}

void GamePreviewView::Render(ImVec2 canvasSize) {

  AudioGlobals::LockAudio();

  auto clockBeatTime = Sequencer::Get().GetClockBeatTime();
  auto frameBeatTime = Sequencer::Get().GetFrameBeatTime();

  auto beatTime = clockBeatTime;
  auto beatFrac = beatTime - std::floor(beatTime);

  AudioGlobals::UnlockAudio();

  HandleInput();

  targetWindowDeltaPos = targetWindowScale;
  targetWindowDeltaNeg = -(targetWindowScale + targetLineExtents.y + fallingNoteExtent.y);

  // Check input
  std::array<double, GameGlobals::kNumGameplayLines> presses = { 0.0f };
  std::array<double, GameGlobals::kNumGameplayLines> releases = { 0.0f };

  gameInput.TakeSnapshot(beatTime, presses, releases);

  // Update line sounds and handle triggering any sounds
  for (size_t gameLineIndex = 0; gameLineIndex < GameGlobals::kNumGameplayLines; ++gameLineIndex) {
    lineTracks[gameLineIndex] = nullptr;

    for (auto noteIter = fallingNotes[gameLineIndex].begin(); noteIter != fallingNotes[gameLineIndex].end(); ++noteIter) {
      if ((*noteIter)->Triggerable()) {
        lineTracks[gameLineIndex] = (*noteIter)->GetTrack();
        break;
      }
    }

    if (presses[gameLineIndex]) {
      auto track = lineTracks[gameLineIndex];
      if (track != nullptr) {
        Sequencer::Get().PlayPatch(track->GetPatch(), track->GetVolume());
      }
    }
  }

  glClearColor(0.2f, 0.2f, 0.7f, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (drawZone) {
    targetZone->SetExtents(glm::vec2(targetLineExtents.x, targetWindowScale * 2.0f));
    targetZone->SetPosition(glm::vec2(targetLinePosition.x,
      (targetLinePosition.y + targetLineExtents.y * 0.5f) - targetWindowScale +
      (beatTimeSlop * kDistanceBetweenQuarterNotes)));
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

  // Update falling notes
  for (uint32 gameLine = 0; gameLine < GameGlobals::kNumGameplayLines; ++gameLine) {
    auto fallingNoteIter = fallingNotes[gameLine].begin();
    while (fallingNoteIter != fallingNotes[gameLine].end()) {
      auto fallingNote = *fallingNoteIter;

      if (fallingNote->Triggerable()) {
       if (presses[gameLine] > 0.0f) {
          auto visualDelta = static_cast<float>(((fallingNote->GetBeatTime() + beatTimeSlop) - presses[gameLine]) * kDistanceBetweenQuarterNotes);

          if (visualDelta < targetWindowDeltaPos) {
            if (visualDelta > targetWindowDeltaNeg) {
              // Trigger it
              fallingNote->Trigger(static_cast<float>(beatTime), true);

              // Position (UL-relative)
              fallingNote->position.y = targetLinePosition.y - fallingNoteExtent.y - visualDelta;

              // Update score
              float score = 0.0f;
              if (std::fabsf(visualDelta) < std::numeric_limits<float>::epsilon()) {
                score += 1.0f;
              }
              else if (visualDelta > 0.0f) {
                score += 1.0f - (visualDelta / targetWindowDeltaPos);
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
        if (!fallingNote->UpdateEffects(static_cast<float>(beatTime))) {
          fallingNoteIter = fallingNotes[gameLine].erase(fallingNoteIter);
          fallingNoteFreeList.Return(fallingNote);
          continue;
        }
      }
      // Otherwise keep dropping it
      else {
        float notePosY = static_cast<float>(targetLinePosition.y - fallingNoteExtent.y -
          (fallingNote->GetBeatTime() - beatTime) * kDistanceBetweenQuarterNotes);
        // Bottom of screen
        if (notePosY >= canvasSize.y) {
          noteStreak = 0;
          streakScore = 0.0f;
          fallingNoteIter = fallingNotes[gameLine].erase(fallingNoteIter);
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
  }

  ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(canvasSize.x), static_cast<float>(canvasSize.y));
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(canvasSize.x - 300.0f, 10.0f));
  ImGui::Begin("Instructions", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  {
    char workBuf[256];
    sprintf(workBuf, "FPS: %03.2f", 1.0f / Globals::elapsedTime);
    ImGui::Text(workBuf);
    sprintf(workBuf, "Beat: %03.2f", beatTime);
    ImGui::Text(workBuf);
    ImGui::Text("Press ESC to exit");
    ImGui::Checkbox("Draw zone", &drawZone);
    ImGui::SliderFloat("Zone scale", &targetWindowScale, targetLineExtents.y * 0.5f, 24.0f);
    ImGui::SliderFloat("Beat slop", &beatTimeSlop, -1.0f, 1.0f);

    for (uint32 lineIndex = 0; lineIndex < GameGlobals::kNumGameplayLines; ++lineIndex) {
      std::string label("Key " + std::to_string(lineIndex + 1));
      if (ImGui::Button(label.c_str())) {
        mappingKey = lineIndex;
      }
      ImGui::SameLine();
      ImGui::Text(SDL_GetKeyName(static_cast<SDL_Keycode>(gameInput.GetLineKey(lineIndex))));
    }

    if (mappingKey != -1) {

      ImGui::Text("Press any key ...");

      auto keyCode = InputState::GetFirstPressedKey();
      if (keyCode != -1) {
        AudioGlobals::LockAudio();
        bool success = gameInput.SetLineKey(mappingKey, keyCode);
        AudioGlobals::UnlockAudio();
        if (success) {
          mappingKey = -1;
        }
      }
    }

    ImGui::Separator();
    sprintf(workBuf, "Score: %03.2f/%03.2f", currentScore, perfectScore);
    ImGui::Text(workBuf);
    std::string streakString("Streak: " + std::to_string(noteStreak));
    ImGui::Text(streakString.c_str());
    sprintf(workBuf, "Streak score: %03.2f/%03.2f", streakScore, static_cast<float>(noteStreak));
    ImGui::Text(workBuf);

    ImGui::End();
  }

  renderable.Render();
}