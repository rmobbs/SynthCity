#pragma once

#include "Renderable.h"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

class SpriteRenderable : public Renderable {
public:
  struct SpriteVert {
    glm::vec2 pos;
    glm::vec2 uv;
    glm::vec4 color;
  };

  uint32 vertexBufferId;

  std::vector<uint32> textures;

  glm::vec2 position = {};
  glm::vec2 extents = {};
  glm::vec4 color = {};

  void Init();
public:
  SpriteRenderable();
  SpriteRenderable(glm::vec2 extents, glm::vec4 color);

  void Render() override;

  void SetPosition(const glm::vec2& position);
  inline const glm::vec2& GetPosition() const {
    return position;
  }

  void SetExtents(const glm::vec2& extents);
  inline const glm::vec2& GetExtents() const {
    return extents;
  }

  void AddTexture(uint32 textureId);
};
