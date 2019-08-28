#pragma once

#include "View.h"
#include "FreeList.h"
#include <list>

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
  float beatTickLength = 0;
  uint32 frameCallbackId = UINT32_MAX;

  void InitResources();
  void TermResources();
  void HandleInput();
  void OnBeat();
public:
  GamePreviewView(uint32 mainWindowHandle);
  ~GamePreviewView();

  void Show() override;
  void Hide() override;
  void Render(ImVec2 canvasSize) override;
};