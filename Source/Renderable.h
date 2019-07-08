#pragma once

#include "BaseTypes.h"
#include <string>
#include <vector>

class ShaderProgram;

class Renderable {
public:
  struct ShaderAttributeInfo {
    std::string name;
    uint32 size;
    uint32 glType;
    uint32 glNormalized;
    uint32 offset;
  };
protected:
  struct ShaderAttribute : ShaderAttributeInfo {
    uint32 location = UINT32_MAX;
    ShaderAttribute(const ShaderAttributeInfo& info)
      : ShaderAttributeInfo(info) {

    }
  };
  std::vector<ShaderAttribute> attributes;
  ShaderProgram* shaderProgram = nullptr;

  void addShaderAttribute(uint32 programId, const std::string& name, uint32 count, uint32 type, uint32 normalized, uint32 offset);
  void bindShaderAttributes(uint32 stride);

public:

  inline void setShaderProgram(ShaderProgram* shaderProgram) {
    this->shaderProgram = shaderProgram;
  }

  virtual void render() = 0;
};

