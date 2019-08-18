#pragma once

#include "Renderable.h"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

class SpriteRenderable : public Renderable {
public:
  struct SpriteVert {
    glm::vec2 pos;
    glm::vec4 color;
  };

  uint32 vertexBufferId;

  glm::vec2 position = {};
  glm::vec2 extents = {};
  glm::vec4 color = {};

  void Init();
public:
  SpriteRenderable();
  SpriteRenderable(glm::vec2 extents, glm::vec4 color);

  void Render() override;
};
