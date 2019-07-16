#pragma once

#include "Renderable.h"

#include <glm/vec2.hpp>

class SpriteRenderable : public Renderable {
public:
  int32 worldScaleId;
  uint32 vertexBufferId;

  glm::vec2 position;
  glm::vec2 extents;

public:
  SpriteRenderable();

  void Render() override;
};
