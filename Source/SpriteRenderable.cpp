#include "SpriteRenderable.h"
#include "GL/glew.h"
#include "GlobalRenderData.h"
#include "ShaderProgram.h"
#include "Logging.h"
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

struct SpriteVert {
  glm::vec2 pos;
  glm::vec2 uv;
  uint32    diffuse;
};

// Sprite quad
static constexpr SpriteVert vertexBufferData[] = {
  // TL anchor
  // TL
  {
    { 0.0f, 0.0f }, { 0.0f, 0.0f }, 0xFFFFFFFF
  },
  // TR
  {
    { 1.0f, 0.0f }, { 0.0f, 0.0f }, 0xFFFFFFFF
  },
  // BR
  {
    { 1.0f, 1.0f }, { 0.0f, 0.0f }, 0xFFFFFFFF
  },
  // BL
  {
    { 0.0f, 1.0f }, { 0.0f, 0.0f }, 0xFFFFFFFF
  },
};

SpriteRenderable::SpriteRenderable() {
  // Generate vertex buffer
  glGenBuffers(1, &vertexBufferId);

  // Get our shader program
  shaderProgram = GlobalRenderData::get().getShaderProgram("SpriteProgram");
  if (shaderProgram != nullptr) {
    // Get the world/scale uniform (TODO: abstract this)
    worldScaleId = glGetUniformLocation(shaderProgram->getProgramId(), "WorldScale");

    // Bind vertex attributes
    addShaderAttribute(shaderProgram->getProgramId(),
      "Position", 2, GL_FLOAT, GL_FALSE, offsetof(SpriteVert, pos));
    addShaderAttribute(shaderProgram->getProgramId(),
      "UV", 2, GL_FLOAT, GL_FALSE, offsetof(SpriteVert, uv));
    addShaderAttribute(shaderProgram->getProgramId(),
      "Color", 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(SpriteVert, diffuse));
  }
  else {
    std::string strError("SpriteRenderable: Unable to find loaded shader program");
    MCLOG(Error, strError.c_str());
    throw std::runtime_error(strError);
  }
}

void SpriteRenderable::render() {
  if (shaderProgram != nullptr) {
    // Save state
    GLint lastArrayBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &lastArrayBuffer);

    shaderProgram->begin();

    glm::mat4x4 worldScale = glm::scale(glm::translate(glm::mat4x4(1.0f),
      glm::vec3(position.x, position.y, 0.0f)),
      glm::vec3(extents.x, extents.y, 0.0f));
    glUniformMatrix4fv(worldScaleId, 1, GL_FALSE, reinterpret_cast<GLfloat*>(&worldScale));

    // Push new state and render
    // glBindBuffer MUST be called before attempting to glEnableVertexAttribArray/glVertexAttribPointer

    // Bind buffers
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexBufferData), vertexBufferData, GL_STATIC_DRAW);

    // Bind buffer offsets to vertex offsets
    bindShaderAttributes(sizeof(SpriteVert));

    glDrawArrays(GL_QUADS, 0, 4);

    shaderProgram->end();

    glBindBuffer(GL_ARRAY_BUFFER, lastArrayBuffer);
  }
}
