#pragma once

#include <glm/mat4x4.hpp>
#include <map>
#include <string>

class ShaderProgram;

class GlobalRenderData {
public:
  enum class MatrixType {
    ScreenOrthographic,
    Count,
  };

protected:
  static constexpr glm::mat4x4 identity4x4 = glm::mat4x4();
  glm::mat4x4 matrix[static_cast<size_t>(MatrixType::Count)];
  std::map<std::string, ShaderProgram> shaderProgramMap;
public:
  static GlobalRenderData& get() {
    static GlobalRenderData rendererData;
    return rendererData;
  }

  void addShaderProgram(const std::string& name, const ShaderProgram&& shaderProgram);

  ShaderProgram* getShaderProgram(const std::string& name);

  void setMatrix(MatrixType matrixType, const glm::mat4x4& newMatrix);
  const glm::mat4x4& getMatrix(MatrixType matrixType);
};

