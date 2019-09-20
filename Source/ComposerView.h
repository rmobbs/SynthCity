#pragma once

#include <string>
#include <vector>
#include <functional>

#include "View.h"
#include "Song.h"
#include "GL/glew.h"
#include "glm/vec4.hpp"
#include <set>
#include <atomic>

class Dialog;
class ComposerView : public View {
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

  uint32 pendingPlayTrack = kInvalidUint32;
  uint32 pendingSoloTrack = kInvalidUint32;
  uint32 pendingCloneTrack = kInvalidUint32;
  uint32 pendingRemoveTrack = kInvalidUint32;
  int32 pendingSubdivision = kInvalidUint32;
  uint32 pendingTempo = kInvalidUint32;
  uint32 pendingAddMeasures = kInvalidUint32;
  bool pendingNewInstrument = false;
  bool pendingLoadInstrument = false;
  bool pendingSaveInstrument = false;
  bool pendingNewSong = false;
  bool pendingLoadSong = false;
  bool pendingSaveSong = false;

  std::map<uint32, std::set<uint32>> noteClipboard;
  std::map<uint32, std::set<uint32>> selectedNotesByTrackId;

  struct SongLine {
    uint32 trackId = kInvalidUint32;
    std::vector<Song::Note*> notes;
    bool mute = false;
  };
  std::map<uint32, SongLine> songLines;
  glm::vec4 dragBox = { -1.0f, -1.0f, -1.0f, -1.0f };
  std::pair<uint32, uint32> toggledNote = { kInvalidUint32, kInvalidUint32 };
  std::pair<uint32, uint32> hoveredNote = { kInvalidUint32, kInvalidUint32 };
  std::pair<uint32, float> pendingTrackVolume = { kInvalidUint32, 0.0f };
  std::pair<uint32, bool> pendingTrackMute = { kInvalidUint32, false };
  uint32 stopButtonIconTexture = 0;
  uint32 pauseButtonIconTexture = 0;
  uint32 logResponderId = UINT32_MAX;
  uint32 mainWindowHandle = UINT32_MAX;
  bool wasConsoleOpen = true;
  Dialog* pendingDialog = nullptr;
  Dialog* activeDialog = nullptr;
  ImGuiRenderable renderable;
  bool songWindowClicked = false;
  uint32 addMeasureCount = 1;
  bool localGuiDisabled = false;
  uint32 soloTrackId = kInvalidUint32;

  void ConditionalEnableBegin(bool condition);
  void ConditionalEnableEnd();
  
  std::map<int, double> playingTrackFlashTimes[2];

  std::atomic<bool> isMetronomeOn = false;
  std::atomic<bool> isLooping = false;

  void InitResources();
  void SetTrackColors(std::string colorScheme, uint32& flashColor);
  void HandleInput();
  void NotePlayedCallback(uint32 trackIndex, uint32 noteIndex);
  void ProcessPendingActions();
  void SelectedGroupAction(std::function<void(uint32, uint32)> action);
  void NewInstrument();
  Instrument* LoadInstrument(std::string requiredInstrument);
  void SaveInstrument();
  void NewSong();
  void LoadSong();
  void SaveSong();
  void RefreshSongLines();
  std::string GetNewInstrumentName(std::string instrumentNameBase);
  std::string GetNewTrackName(std::string trackNameBase);

public:
  ComposerView(uint32 mainWindowHandle);
  ~ComposerView();

  void Render(ImVec2 canvasSize) override;

  void Show() override;
  void Hide() override;

  void OnBeat(uint32 beatIndex) override;
};
