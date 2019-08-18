#pragma once

#include "Renderable.h"
#include "imgui.h"

class ImGuiRenderable : public Renderable {
protected:
  uint32 vertexBufferId = UINT32_MAX;
  uint32 vertexBufferId2 = UINT32_MAX;
  uint32 elementBufferId = UINT32_MAX;
  uint32 fontTextureLoc = UINT32_MAX;

public:
  ImGuiRenderable();

  void Render() override;
};
