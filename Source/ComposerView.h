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
class InstrumentInstance;

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

  template<typename T> class InstrumentTrackData {
  public:
    Instrument* instrument = nullptr;
    uint32 trackId = kInvalidUint32;
    T data = 0;

    inline bool operator==(const InstrumentTrackData<T>& that) const {
      return instrument == that.instrument && trackId == that.trackId && data == that.data;
    }
    inline bool operator!=(const InstrumentTrackData<T>& that) const {
      return !operator==(that);
    }
  };

  template<typename T> class InstrumentInstanceTrackData {
  public:
    InstrumentInstance* instance = nullptr;
    uint32 trackId = kInvalidUint32;
    T data = 0;

    inline bool operator==(const InstrumentInstanceTrackData<T>& that) const {
      return instance == that.instance && trackId == that.trackId && data == that.data;
    }
    inline bool operator!=(const InstrumentInstanceTrackData<T>& that) const {
      return !operator==(that);
    }
  };

  bool wasPlaying = false;

  InstrumentTrackData<int32> pendingPlayTrack;
  InstrumentTrackData<int32> pendingCloneTrack;
  InstrumentTrackData<int32> pendingRemoveTrack;

  InstrumentInstanceTrackData<int32> pendingSoloTrackInstance;
  InstrumentInstanceTrackData<int32> soloTrackInstance;
  InstrumentInstanceTrackData<int32> pendingMoveInstrumentInstance;
  InstrumentInstanceTrackData<uint32> pendingToggleNoteInstance;
  InstrumentInstanceTrackData<uint32> hoveredNoteInstance;
  InstrumentInstanceTrackData<float> pendingVolumeTrackInstance;
  InstrumentInstanceTrackData<bool> pendingMuteTrackInstance;

  InstrumentInstance* pendingRemoveInstrumentInstance = nullptr;
  InstrumentInstance* pendingSaveInstrument = nullptr;

  int32 pendingSubdivision = kInvalidUint32;
  uint32 pendingTempo = kInvalidUint32;
  uint32 pendingAddMeasures = kInvalidUint32;
  bool pendingNewInstrument = false;
  bool pendingLoadInstrument = false;
  bool pendingNewSong = false;
  bool pendingLoadSong = false;
  bool pendingSaveSong = false;

  std::map<InstrumentInstance*, std::map<uint32, std::set<uint32>>> selectedNotesByInstrumentInstance;
  std::map<InstrumentInstance*, std::map<uint32, std::set<uint32>>> selectingNotesByInstrumentInstance;

  glm::vec4 dragBox = { -1.0f, -1.0f, -1.0f, -1.0f };
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
  void SelectedGroupAction(std::function<void(InstrumentInstance*, uint32, uint32)> action);
  void NewInstrument();
  Instrument* LoadInstrument(std::string requiredInstrument);
  void SaveInstrument(Instrument* instrument);
  void NewSong();
  void LoadSong();
  void SaveSong();
  std::string GetUniqueInstrumentName(std::string instrumentNameBase);
  std::string GetUniqueInstrumentInstanceName(std::string instrumentInstanceNameBase);
  std::string GetUniqueTrackName(Instrument* instrument, std::string trackNameBase);

public:
  ComposerView(uint32 mainWindowHandle);
  ~ComposerView();

  void Render(ImVec2 canvasSize) override;

  void Show() override;
  void Hide() override;

  void OnBeat(uint32 beatIndex) override;
};
