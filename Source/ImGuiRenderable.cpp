#include "ImGuiRenderable.h"
#include "imgui.h"
#include "GlobalRenderData.h"
#include "ShaderProgram.h"
#include "Logging.h"
#include <stdexcept>
#include "GL/glew.h"

ImGuiRenderable::ImGuiRenderable() {
  // Generate ImGui VB and EB
  glGenBuffers(1, &vertexBufferId);
  glGenBuffers(1, &elementBufferId);

  // Get our shader program
  shaderProgram = GlobalRenderData::get().getShaderProgram("ImGuiProgram");
  if (shaderProgram != nullptr) {
    // Get the world/scale uniform (TODO: abstract this)
    fontTextureLoc = shaderProgram->GetUniform("Texture").location;

    // Bind vertex attributes
    addShaderAttribute(shaderProgram->getProgramId(),
      "Position", 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, pos));
    addShaderAttribute(shaderProgram->getProgramId(),
      "UV", 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, uv));
    addShaderAttribute(shaderProgram->getProgramId(),
      "Color", 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(ImDrawVert, col));
  }
  else {
    std::string strError("ImGuiRenderable: Unable to find loaded shader program");
    MCLOG(Error, strError.c_str());
    throw std::runtime_error(strError);
  }
#if 0
  glGenBuffers(1, &vertexBufferId2);
#endif
}

void ImGuiRenderable::Render() {
#if 0
  static const GLfloat g_vertex_buffer_data[] = {
    -1.0f, -1.0f, 0.0f,
    1.0f, -1.0f, 0.0f,
    0.0f,  1.0f, 0.0f,
  };

  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId2);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);
  glVertexAttribPointer(
    0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
    3,                  // size
    GL_FLOAT,           // type
    GL_FALSE,           // normalized?
    0,                  // stride
    (void*)0            // array buffer offset
  );
  // Draw the triangle !
  glDrawArrays(GL_TRIANGLES, 0, 3); // Starting from vertex 0; 3 vertices total -> 1 triangle
  glDisableVertexAttribArray(0);
#endif

  if (shaderProgram != nullptr) {
    ImGui::Render();

    ImDrawData* imDrawData = ImGui::GetDrawData();

    // Backup GL state
    GLboolean lastEnableBlend = glIsEnabled(GL_BLEND);
    GLboolean lastEnableCullFace = glIsEnabled(GL_CULL_FACE);
    GLboolean lastEnableDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean lastEnableScissorTest = glIsEnabled(GL_SCISSOR_TEST);

    GLint lastTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);
    GLint lastArrayBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &lastArrayBuffer);
    GLint lastElementArrayBuffer;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &lastElementArrayBuffer);

    // Set our state
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    auto& imGuiIo = ImGui::GetIO();
    glViewport(0, 0, static_cast<GLuint>(imGuiIo.
      DisplaySize.x), static_cast<GLuint>(imGuiIo.DisplaySize.y));

    // Handle issue where screen coordinates and framebuffer coordinates are not 1:1
    auto frameBufferHeight = imGuiIo.DisplaySize.y * imGuiIo.DisplayFramebufferScale.y;
    imDrawData->ScaleClipRects(imGuiIo.DisplayFramebufferScale);

    shaderProgram->begin();

    // Select the 0th texture
    glUniform1i(fontTextureLoc, 0);

    // Run through the ImGui draw lists
    for (int n = 0; n < imDrawData->CmdListsCount; ++n) {
      const auto& cmdList = imDrawData->CmdLists[n];

      // Bind vertex buffer and set vertex data
      glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
      glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cmdList->VtxBuffer.size()) *
        sizeof(ImDrawVert), reinterpret_cast<GLvoid*>(&cmdList->VtxBuffer.front()), GL_STREAM_DRAW);

      // Bind buffer offsets to vertex offsets
      bindShaderAttributes(sizeof(ImDrawVert));

      // Bind index buffer and set index data
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferId);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(cmdList->IdxBuffer.size()) *
        sizeof(ImDrawIdx), reinterpret_cast<GLvoid*>(&cmdList->IdxBuffer.front()), GL_STREAM_DRAW);

      // Run through the draw commands for this draw list
      const ImDrawIdx* drawIndex = 0;
      for (const auto& drawCmd : cmdList->CmdBuffer) {
        if (drawCmd.UserCallback != nullptr) {
          drawCmd.UserCallback(cmdList, &drawCmd);
        }
        else {
          glBindTexture(GL_TEXTURE_2D, reinterpret_cast<GLuint>(drawCmd.TextureId));
          glScissor(static_cast<int>(drawCmd.ClipRect.x),
            static_cast<int>(frameBufferHeight - drawCmd.ClipRect.w),
            static_cast<int>(drawCmd.ClipRect.z - drawCmd.ClipRect.x),
            static_cast<int>(drawCmd.ClipRect.w - drawCmd.ClipRect.y));
          glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(drawCmd.ElemCount), GL_UNSIGNED_SHORT, drawIndex);
        }
        drawIndex += drawCmd.ElemCount;
      }
    }

    shaderProgram->end();

    // Restore previous GL state
    glBindTexture(GL_TEXTURE_2D, lastTexture);
    glBindBuffer(GL_ARRAY_BUFFER, lastArrayBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lastElementArrayBuffer);

    if (lastEnableBlend) {
      glEnable(GL_BLEND);
    }
    else {
      glDisable(GL_BLEND);
    }
    if (lastEnableCullFace) {
      glEnable(GL_CULL_FACE);
    }
    else {
      glDisable(GL_CULL_FACE);
    }
    if (lastEnableDepthTest) {
      glEnable(GL_DEPTH_TEST);
    }
    else {
      glDisable(GL_DEPTH_TEST);
    }
    if (lastEnableScissorTest) {
      glEnable(GL_SCISSOR_TEST);
    }
    else {
      glDisable(GL_SCISSOR_TEST);
    }
  }
}


