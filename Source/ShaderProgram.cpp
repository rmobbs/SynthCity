#include "ShaderProgram.h"
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

ShaderProgram::ShaderProgram(const std::string& programName, const std::string& vertPath, const std::string& fragPath, const std::vector<UniformInfo>& uniformInfo) {
  this->programName = programName;

  this->vertShaderId = CompileShaderFromFile(vertPath, GL_VERTEX_SHADER);
  if (this->vertShaderId == UINT32_MAX) {
    throw std::runtime_error("<failure>");
  }

  this->fragShaderId = CompileShaderFromFile(fragPath, GL_FRAGMENT_SHADER);
  if (this->fragShaderId == UINT32_MAX) {
    glDeleteShader(this->vertShaderId);
    this->vertShaderId = UINT32_MAX;
    throw std::runtime_error("<failure>");
  }

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

  // Locate uniforms
  for (const auto& info : uniformInfo) {
    Uniform uniform(info);
    uniform.location = glGetUniformLocation(this->programId, info.name.c_str());
    uniforms.emplace_back(uniform);
  }
}

void ShaderProgram::begin() {
  glGetIntegerv(GL_CURRENT_PROGRAM, &this->previousProgramId);
  glUseProgram(this->programId);

  // Bind auto-bind uniforms
  for (const auto& uniform : this->uniforms) {
    switch (uniform.type) {
      case UniformInfo::Type::Matrix4x4: {
        auto data = reinterpret_cast<const GLfloat*>(uniform.data());
        glUniformMatrix4fv(uniform.location, 1, GL_FALSE,
          data);
        break;
      }
    }
  }
}

void ShaderProgram::end() {
  if (this->previousProgramId != -1) {
    glUseProgram(this->previousProgramId);
  }
}
