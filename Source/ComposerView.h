#pragma once

#include <string>
#include <vector>
#include <functional>

#include "View.h"
#include "GL/glew.h"
#include "glm/vec4.hpp"
#include <set>

class Dialog;
class ComposerView : public View {
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
  int32 pendingSubdivision = -1;
  int32 pendingTempo = -1;
  float pendingMasterVolume = -1.0f;
  bool pendingNewInstrument = false;
  bool pendingLoadInstrument = false;
  bool pendingSaveInstrument = false;
  bool pendingNewSong = false;
  bool pendingLoadSong = false;
  bool pendingSaveSong = false;

  uint32 notePlayedCallbackId = UINT32_MAX;
  std::vector<std::set<uint32>> noteClipboard;
  std::vector<std::set<uint32>> noteSelectedStatus;
  glm::vec4 dragBox = { -1.0f, -1.0f, -1.0f, -1.0f };
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
  bool songWindowClicked = false;

  std::map<int, double> playingTrackFlashTimes[2];
  Mode mode = Mode::Normal;

  void InitResources();
  void SetTrackColors(std::string colorScheme, uint32& flashColor);
  void HandleInput();
  void NotePlayedCallback(uint32 trackIndex, uint32 noteIndex);
  void ProcessPendingActions();
  void ClearSelectedNotes();
  void SelectedGroupAction(std::function<void(int32, int32)> action);

public:
  ComposerView(uint32 mainWindowHandle);
  ~ComposerView();

  void Render(ImVec2 canvasSize) override;

  void Show() override;
  void Hide() override;
};
