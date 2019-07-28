#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

#include "ImGuiRenderable.h"
#include "GL/glew.h"
#include "imgui.h"

class Dialog;
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

  OutputWindowState outputWindowState;

  bool wasPlaying = false;
  int32 pendingSoloTrack = -1;
  int32 pendingPlayTrack = -1;
  uint32 playButtonIconTexture = 0;
  uint32 stopButtonIconTexture = 0;
  uint32 pauseButtonIconTexture = 0;
  uint32 logResponderId = UINT32_MAX;
  uint32 mainWindowHandle = UINT32_MAX;
  bool wasConsoleOpen = true;
  Dialog* pendingDialog = nullptr;
  Dialog* activeDialog = nullptr;
  ImGuiRenderable renderable;

  std::map<int, double> playingTrackFlashTimes[2];
  std::map<int, double> playingNotesFlashTimes[2];
  std::function<void()> exitFunction;

  void InitResources();
  void SetTrackColors(std::string colorScheme, uint32& flashColor);

public:
  ComposerView(uint32 mainWindowHandle, std::function<void()> exitFunction);
  ~ComposerView();

  void Render(double currentTime, ImVec2 canvasSize);
};
