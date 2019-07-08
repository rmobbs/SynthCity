#pragma once

#include "BaseTypes.h"
#include <string>
#include <limits>
#include <vector>
#include <map>
#include <functional>

class ShaderProgram {
public:
  struct UniformInfo {
    enum class Type {
      Matrix4x4,
    };
    std::string name;
    Type type;
    std::function<const void*()> data;
  };
protected:
  struct Uniform : UniformInfo {
    int32 location;

    Uniform(const UniformInfo& info)
      : UniformInfo(info)
      , location(-1) {

    }
  };
protected:
  uint32 CompileShaderFromFile(const std::string& shaderPath, uint32 shaderType);

  uint32 vertShaderId = UINT32_MAX;
  uint32 fragShaderId = UINT32_MAX;
  uint32 programId = UINT32_MAX;
  int32 previousProgramId = -1;
  std::string programName;

  std::vector<Uniform> uniforms;

public:
  ShaderProgram(const std::string& programName, const std::string& vertPath, const std::string& fragPath, const std::vector<UniformInfo>& uniformInfo);

  uint32 getProgramId() const {
    return programId;
  }

  void begin();
  void end();
};

