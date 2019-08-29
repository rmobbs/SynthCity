#include "SpriteRenderable.h"
#include "GL/glew.h"
#include "GlobalRenderData.h"
#include "ShaderProgram.h"
#include "Logging.h"
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

// Sprite quad
static SpriteRenderable::SpriteVert vertexBufferData[] = {
  // TL anchor
  // TL
  {
    { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }
  },
  // TR
  {
    { 1.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }
  },
  // BR
  {
    { 1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }
  },
  // BL
  {
    { 0.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }
  },
};

SpriteRenderable::SpriteRenderable() {
  Init();
}

SpriteRenderable::SpriteRenderable(glm::vec2 extents, glm::vec4 color)
  : extents(extents)
  , color(color) {
  Init();
}

void SpriteRenderable::SetPosition(const glm::vec2& position) {
  this->position = position;
}

void SpriteRenderable::SetExtents(const glm::vec2& extents) {
  this->extents = extents;
}

void SpriteRenderable::AddTexture(uint32 textureId) {
  textures.push_back(textureId);
}

void SpriteRenderable::Init() {
  // Generate vertex buffer
  glGenBuffers(1, &vertexBufferId);

  // Get our shader program
  shaderProgram = GlobalRenderData::get().getShaderProgram("TexturedDiffuse2D");
  if (shaderProgram != nullptr) {
    // Bind vertex attributes
    addShaderAttribute(shaderProgram->getProgramId(),
      "Position", 2, GL_FLOAT, GL_FALSE, offsetof(SpriteRenderable::SpriteVert, pos));
    addShaderAttribute(shaderProgram->getProgramId(),
      "UV", 2, GL_FLOAT, GL_FALSE, offsetof(SpriteRenderable::SpriteVert, uv));
    addShaderAttribute(shaderProgram->getProgramId(),
      "Color", 4, GL_FLOAT, GL_TRUE, offsetof(SpriteRenderable::SpriteVert, color));
  }
  else {
    std::string strError("SpriteRenderable: Unable to find loaded shader program");
    MCLOG(Error, strError.c_str());
    throw std::runtime_error(strError);
  }
}

void SpriteRenderable::Render() {
  if (shaderProgram != nullptr) {
    // Save state
    GLint lastArrayBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &lastArrayBuffer);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    for (size_t t = 0; t < textures.size(); ++t) {
      glActiveTexture(GL_TEXTURE0 + t);
      glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textures[t]));
    }

    shaderProgram->begin();

    glm::mat4x4 worldScale = glm::scale(glm::translate(glm::mat4x4(1.0f),
      glm::vec3(position.x, position.y, 0.0f)),
      glm::vec3(extents.x, extents.y, 0.0f));
    glUniformMatrix4fv(shaderProgram->GetUniform("WorldScale").
      location, 1, GL_FALSE, reinterpret_cast<GLfloat*>(&worldScale));

    // Push new state and render
    // glBindBuffer MUST be called before attempting to glEnableVertexAttribArray/glVertexAttribPointer

    for (uint32 i = 0; i < _countof(vertexBufferData); ++i) {
      vertexBufferData[i].color = color;
    }

    // Bind buffers
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexBufferData), vertexBufferData, GL_STATIC_DRAW);

    // Bind buffer offsets to vertex offsets
    bindShaderAttributes(sizeof(SpriteRenderable::SpriteVert));

    glDrawArrays(GL_QUADS, 0, 4);

    shaderProgram->end();

    glBindBuffer(GL_ARRAY_BUFFER, lastArrayBuffer);
  }
}
