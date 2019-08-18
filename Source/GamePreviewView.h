#pragma once

#include "View.h"
#include "FreeList.h"
#include <queue>

class SpriteRenderable;
class GamePreviewView : public View {
protected:
  ImGuiRenderable renderable;
  uint32 mainWindowHandle = UINT32_MAX;
  std::vector<SpriteRenderable*> staticSprites;
  std::queue<SpriteRenderable*> dynamicSprites;

  void InitResources();
  void TermResources();
  void HandleInput();
public:
  GamePreviewView(uint32 mainWindowHandle);
  ~GamePreviewView();

  void Show() override;
  void Render(ImVec2 canvasSize) override;
};