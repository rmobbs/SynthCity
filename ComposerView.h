#pragma once

#include <string>
#include <vector>
#include <map>

#include "Renderable.h"
#include "GL/glew.h"
#include "imgui.h"

class ComposerView {
protected:
  class  OutputWindowState {
  public:
    std::vector<std::string> displayHistory;
    bool scrollToBottom = false;
    bool autoScroll = true;

    void ClearLog();
    void AddLog(const char* fmt, ...);
    void AddLog(const std::string_view& logString);
  };

  // Renderable to drive our shader program
  class ImGuiRenderable : public Renderable {
  protected:
    GLuint vertexBufferId = UINT32_MAX;
    GLuint elementBufferId = UINT32_MAX;
    GLuint fontTextureLoc = UINT32_MAX;

  public:
    ImGuiRenderable();

    // Rethink
    void render() override { }

    void Render();

    void SetTrackColors(std::string colorScheme, uint32& flashColor);
  };

  OutputWindowState outputWindowState;

  GLuint fontTextureId;
  int32 pendingPlayTrack = -1;
  GLuint playButtonIconTexture = 0;
  GLuint stopButtonIconTexture = 0;
  GLuint pauseButtonIconTexture = 0;
  uint32 logResponderId = UINT32_MAX;
  ImGuiRenderable imGuiRenderable;

  std::map<int, double> playingTrackFlashTimes[2];
  std::map<int, double> playingNotesFlashTimes[2];

  void InitResources();

public:
  ComposerView();
  ~ComposerView();

  void Render(double currentTime, ImVec2 canvasSize);
};
