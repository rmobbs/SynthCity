#pragma once

#include "View.h"
#include "FreeList.h"
#include "glm/vec2.hpp"
#include "GameGlobals.h"
#include "GameInput.h"
#include "Song.h"

#include <list>
#include <array>

class SpriteRenderable;
class NoteSprite;
class Track;
class GamePreviewView : public View {
protected:
  enum class Mode {
    Ready,
    Countdown_12,
    Countdown_34,
    Countdown_1,
    Countdown_2,
    Countdown_3,
    Countdown_4,
    Playing,
    Done,
  };
  Mode mode = Mode::Ready;
  ImGuiRenderable renderable;
  uint32 mainWindowHandle = UINT32_MAX;
  std::vector<SpriteRenderable*> staticSprites;
  std::vector<SpriteRenderable*> fretSprites;
  std::array<Track*, GameGlobals::kNumGameplayLines> lineTracks = { nullptr };
  std::array<std::list<NoteSprite*>, GameGlobals::kNumGameplayLines> fallingNotes;
  std::vector<SpriteRenderable*> countdownPlacards;

  struct LineState {
    Track* track = nullptr;
    std::list<Song::Note>::const_iterator cur;
    std::list<Song::Note>::const_iterator end;
  };
  std::vector<LineState> autoNotes;
  SpriteRenderable* readyPlacard = nullptr;
  SpriteRenderable* targetZone = nullptr;
  std::vector<uint32> loadedTextures;
  uint32 fallingNoteTextureId;
  uint32 whiteTextureId;
  glm::vec2 whiteTextureSize;
  GameInput gameInput;
  float targetWindowScale = 0.0f;
  float targetWindowDeltaPos = 0.0f;
  float targetWindowDeltaNeg = 0.0f;
  float currentScore = 0.0f;
  float perfectScore = 0.0f;
  float streakScore = 0.0f;
  float beatTimeSlop = 0.0f;
  bool drawZone = false;
  int32 mappingKey = -1;
  uint32 introBeats = 0;
  uint32 noteStreak;

  void InitResources();
  void TermResources();
  void HandleInput();
  uint32 LoadTexture(const std::string& textureName, uint32* outWidth = nullptr, uint32* outHeight = nullptr);
public:
  GamePreviewView(uint32 mainWindowHandle);
  ~GamePreviewView();

  void Show() override;
  void Hide() override;
  void Render(ImVec2 canvasSize) override;

  void OnBeat(uint32 beatIndex) override;
};