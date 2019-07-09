#include "Renderable.h"
#include "GL/glew.h"


void Renderable::addShaderAttribute(uint32 programId, const std::string& name, uint32 count, uint32 type, uint32 normalized, uint32 offset) {
  auto location = glGetAttribLocation(programId, name.c_str());
  if (location != -1) {
    ShaderAttribute attribute({ name, count, type, normalized, offset });
    attribute.location = location;
    attributes.push_back(attribute);
  }
}

void Renderable::bindShaderAttributes(uint32 stride) {
  for (const auto& attrib : attributes) {
    glEnableVertexAttribArray(attrib.location);
    glVertexAttribPointer(attrib.location, attrib.size, attrib.glType,
      attrib.glNormalized, stride, reinterpret_cast<GLvoid*>(attrib.offset));
  }
}

