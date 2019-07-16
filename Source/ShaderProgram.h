#pragma once

#include "BaseTypes.h"
#include <string>
#include <limits>
#include <vector>
#include <map>
#include <functional>

class ShaderProgram {
public:
  struct Attribute {
    std::string name;
    int32 location;
  };
  struct Uniform {
    std::string name;
    int32 location;
    std::function<void(int32 location)> data;
  };
protected:
  uint32 CompileShaderFromFile(const std::string& shaderPath, uint32 shaderType);

  uint32 vertShaderId = UINT32_MAX;
  uint32 fragShaderId = UINT32_MAX;
  uint32 programId = UINT32_MAX;
  int32 previousProgramId = -1;
  std::string programName;

  std::map<std::string, Uniform> uniformMap;
  std::map<std::string, Attribute> attributeMap;

public:
  ShaderProgram(const std::string& programName, const std::string& vertPath, const std::string& fragPath, const std::vector<Uniform>& uniforms = {}, const std::vector<Attribute>& attributes = {});

  uint32 getProgramId() const {
    return programId;
  }

  void begin();
  void end();

  const Uniform& GetUniform(std::string uniformName) const;
  const Attribute& GetAttribute(std::string attributeName) const;
};

