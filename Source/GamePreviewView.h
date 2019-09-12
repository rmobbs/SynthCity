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
    Countdown,
    Playing,
    Done,
  };
  Mode mode = Mode::Ready;
  uint32 beatsLeftInState = 0;
  ImGuiRenderable renderable;
  uint32 mainWindowHandle = UINT32_MAX;
  std::vector<SpriteRenderable*> staticSprites;
  std::vector<SpriteRenderable*> fretSprites;
  std::list<SpriteRenderable*> fallingNotes;
  std::vector<SpriteRenderable*> countdownPlacards;
  SpriteRenderable* readyPlacard = nullptr;
  float beatFrameLength = 0;
  uint32 beatCallbackId = UINT32_MAX;
  std::vector<uint32> loadedTextures;
  uint32 fallingNoteTextureId;
  glm::vec2 fallingNoteTextureSize;
  uint32 whiteTextureId;
  glm::vec2 whiteTextureSize;

  void InitResources();
  void TermResources();
  void HandleInput();
  void OnBeat();
  uint32 LoadTexture(const std::string& textureName, uint32* outWidth = nullptr, uint32* outHeight = nullptr);
public:
  GamePreviewView(uint32 mainWindowHandle);
  ~GamePreviewView();

  void Show() override;
  void Hide() override;
  void Render(ImVec2 canvasSize) override;
};