#pragma once

#include "View.h"
#include "FreeList.h"
#include <list>
#include "glm//vec2.hpp"

class SpriteRenderable;
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
  std::list<SpriteRenderable*> fallingNotes;
  std::vector<SpriteRenderable*> countdownPlacards;
  SpriteRenderable* readyPlacard = nullptr;
  SpriteRenderable* targetZone = nullptr;
  uint32 beatCallbackId = UINT32_MAX;
  std::vector<uint32> loadedTextures;
  uint32 fallingNoteTextureId;
  uint32 whiteTextureId;
  glm::vec2 whiteTextureSize;
  float targetWindowRush = 0.0f;
  float targetWindowDrag = 0.0f;
  bool leaveRefuse = false;

  void InitResources();
  void TermResources();
  void HandleInput();
  void OnBeat(uint32 beat);
  uint32 LoadTexture(const std::string& textureName, uint32* outWidth = nullptr, uint32* outHeight = nullptr);
public:
  GamePreviewView(uint32 mainWindowHandle);
  ~GamePreviewView();

  void Show() override;
  void Hide() override;
  void Render(ImVec2 canvasSize) override;
};