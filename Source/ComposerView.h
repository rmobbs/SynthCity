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
  enum class Mode {
    Normal,
    Markup,
  };
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

  int32 pendingSoloTrack = -2;
  int32 pendingPlayTrack = -1;
  int32 pendingRemoveTrack = -1;
  int32 pendingCloneTrack = -1;
  int32 pendingNumMeasures = -1;
  int32 pendingSubdivision = -1;
  int32 pendingBeatsPerMinute = -1;
  int32 pendingBeatsPerMeasure = -1;
  float pendingMasterVolume = -1.0f;
  std::pair<int32, int32> toggledNote = { -1, -1 };
  std::pair<int32, int32> hoveredNote = { -1, -1 };
  std::pair<int32, float> pendingTrackVolume = { -1, 0.0f };
  std::pair<int32, bool> pendingTrackMute = { -1, false };
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
  Mode mode = Mode::Normal;

  void InitResources();
  void SetTrackColors(std::string colorScheme, uint32& flashColor);
  void HandleInput();
  void NotePlayedCallback(uint32 trackIndex, uint32 noteIndex);
  void ProcessPendingActions();

public:
  ComposerView(uint32 mainWindowHandle, std::function<void()> exitFunction);
  ~ComposerView();

  void Render(ImVec2 canvasSize);
};
