#include "GlobalRenderData.h"
#include "ShaderProgram.h"

void GlobalRenderData::addShaderProgram(const std::string& name, const ShaderProgram&& shaderProgram) {
  shaderProgramMap.emplace(name, shaderProgram);
}

ShaderProgram* GlobalRenderData::getShaderProgram(const std::string& name) {
  const auto& shaderProgram = shaderProgramMap.find(name);
  if (shaderProgram != shaderProgramMap.end()) {
    return &shaderProgram->second;
  }
  return nullptr;
}

void GlobalRenderData::setMatrix(MatrixType matrixType, const glm::mat4x4& newMatrix) {
  size_t matrixIndex = static_cast<size_t>(matrixType);
  if (matrixIndex < static_cast<size_t>(MatrixType::Count)) {
    matrix[matrixIndex] = newMatrix;
  }
}
const glm::mat4x4& GlobalRenderData::getMatrix(MatrixType matrixType) {
  size_t matrixIndex = static_cast<size_t>(matrixType);
  if (matrixIndex < static_cast<size_t>(MatrixType::Count)) {
    return matrix[matrixIndex];
  }
  return identity4x4;
}

uint32 GlobalRenderData::GetShader(std::string name) {
  auto const shaderEntry = shaderMap.find(name);
  if (shaderEntry != shaderMap.end()) {
    return shaderEntry->second;
  }
  return UINT32_MAX;
}

void GlobalRenderData::AddShader(std::string name, uint32 shaderId) {
  shaderMap[name] = shaderId;
}

