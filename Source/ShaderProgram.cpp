#include "ShaderProgram.h"
#include "GlobalRenderData.h"
#include <istream>
#include <fstream>
#include <sstream>
#include <vector>

#include "GL/glew.h"

#include "Logging.h"


uint32 ShaderProgram::CompileShaderFromFile(const std::string& shaderPath, uint32 shaderType) {
  GLuint shaderId = UINT32_MAX;
  if (shaderPath.length() > 0) {
    // Read
    std::ifstream shaderStream(shaderPath, std::ios::in);
    if (shaderStream.is_open()) {
      std::stringstream ss;
      ss << shaderStream.rdbuf();
      shaderStream.close();

      auto shaderCode = ss.str();
      if (shaderCode.length() > 0) {
        shaderId = glCreateShader(shaderType);

        char const* rawShaderCode = shaderCode.c_str();
        glShaderSource(shaderId, 1, &rawShaderCode, nullptr);
        glCompileShader(shaderId);

        // Check status
        GLint status = GL_FALSE;
        glGetShaderiv(shaderId, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
          std::string errorLog = std::string("Unable to compile shader \'") + shaderPath + std::string("'");

          GLint infoLogLength = -1;
          glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLogLength);
          if (infoLogLength > 0) {
            std::vector<GLchar> infoLog(infoLogLength + 1);
            glGetShaderInfoLog(shaderId, infoLogLength, nullptr, &infoLog.front());
            errorLog += std::string(&infoLog.front(), infoLogLength);
          }

          MCLOG(Error, "%s", errorLog.c_str());

          glDeleteShader(shaderId);
          shaderId = UINT32_MAX;
        }
      }
    }
    else {
      MCLOG(Error, "Unable to load shader file %s", shaderPath.c_str());
    }
  }
  return shaderId;
}

ShaderProgram::ShaderProgram(const std::string& programName, const std::string& vertPath, const std::string& fragPath, const std::vector<Uniform>& uniforms, const std::vector<Attribute>& attributes) {
  this->programName = programName;

  auto vertShaderId = GlobalRenderData::get().GetShader(vertPath);
  if (vertShaderId == UINT32_MAX) {
    vertShaderId = CompileShaderFromFile(vertPath, GL_VERTEX_SHADER);
    if (vertShaderId == UINT32_MAX) {
      throw std::runtime_error("<failure>");
    }
    GlobalRenderData::get().AddShader(vertPath, vertShaderId);
  }
  this->vertShaderId = vertShaderId;

  auto fragShaderId = GlobalRenderData::get().GetShader(fragPath);
  if (fragShaderId == UINT32_MAX) {
    fragShaderId = CompileShaderFromFile(fragPath, GL_FRAGMENT_SHADER);
    if (fragShaderId == UINT32_MAX) {
      throw std::runtime_error("<failure>");
    }
    GlobalRenderData::get().AddShader(fragPath, fragShaderId);
  }
  this->fragShaderId = fragShaderId;

  this->programId = glCreateProgram();
  glAttachShader(this->programId, this->vertShaderId);
  glAttachShader(this->programId, this->fragShaderId);
  glLinkProgram(this->programId);

  // Check status
  GLint status = GL_FALSE;
  glGetProgramiv(this->programId, GL_LINK_STATUS, &status);
  if (status == GL_FALSE) {
    std::string errorLog = std::string("Unable to link program \'") + this->programName + std::string("'");

    GLint infoLogLength = -1;
    glGetShaderiv(this->programId, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 0) {
      std::vector<GLchar> infoLog(infoLogLength + 1);
      glGetShaderInfoLog(this->programId, infoLogLength, nullptr, &infoLog.front());
      errorLog += std::string(&infoLog.front(), infoLogLength);
    }

    MCLOG(Error, "%s", errorLog.c_str());
    throw std::runtime_error("<failure>");
  }

  for (const auto& uniform : uniforms) {
    uniformMap[uniform.name] = uniform;
  }
  for (const auto& attribute : attributes) {
    attributeMap[attribute.name] = attribute;
  }

}

void ShaderProgram::begin() {
  glGetIntegerv(GL_CURRENT_PROGRAM, &this->previousProgramId);
  glUseProgram(this->programId);

  // Bind auto-bind uniforms
  for (const auto& uniform : this->uniformMap) {
    if (uniform.second.data != nullptr) {
      uniform.second.data(uniform.second.location);
    }
  }
}

void ShaderProgram::end() {
  if (this->previousProgramId != -1) {
    glUseProgram(this->previousProgramId);
  }
}

const ShaderProgram::Uniform& ShaderProgram::GetUniform(std::string uniformName) const {
  return uniformMap.find(uniformName)->second;
}

const ShaderProgram::Attribute& ShaderProgram::GetAttribute(std::string attributeName) const {
  return attributeMap.find(attributeName)->second;
}
