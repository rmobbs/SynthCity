#pragma once

#include "View.h"
#include "Instrument.h"
#include "InstrumentInstance.h"
#include "glm/vec4.hpp"
#include <atomic>
#include <set>

class ComposerView;
class SongTab : public View {
public:
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

  InstrumentInstanceTrackData<int32> pendingPlayTrackInstance;
  InstrumentInstanceTrackData<bool> pendingSoloTrackInstance;
  InstrumentInstanceTrackData<int32> pendingMoveInstrumentInstance;
  InstrumentInstanceTrackData<uint32> pendingToggleNoteInstance;
  InstrumentInstanceTrackData<uint32> hoveredNoteInstance;
  InstrumentInstanceTrackData<bool> pendingMuteTrackInstance;

  InstrumentInstance* pendingCreateInstrumentInstance = nullptr;
  InstrumentInstance* pendingRemoveInstrumentInstance = nullptr;

  int32 pendingSubdivision = kInvalidUint32;
  uint32 pendingTempo = kInvalidUint32;
  uint32 pendingAddMeasures = kInvalidUint32;
  bool pendingAddInstrument = false;
  bool pendingNewSong = false;
  bool pendingLoadSong = false;
  bool pendingSaveSong = false;

  std::map<InstrumentInstance*, std::map<uint32, std::set<uint32>>> selectedNotesByInstrumentInstance;
  std::map<InstrumentInstance*, std::map<uint32, std::set<uint32>>> selectingNotesByInstrumentInstance;

  glm::vec4 dragBox = { -1.0f, -1.0f, -1.0f, -1.0f };
  uint32 stopButtonIconTexture = 0;
  uint32 pauseButtonIconTexture = 0;
  Dialog* pendingDialog = nullptr;
  Dialog* activeDialog = nullptr;
  ImGuiRenderable renderable;
  bool songWindowClicked = false;
  uint32 addMeasureCount = 1;
  bool localGuiDisabled = false;
  ComposerView* composerView = nullptr;

  void ConditionalEnableBegin(bool condition);
  void ConditionalEnableEnd();

  std::map<int, double> playingTrackFlashTimes[2];

  void InitResources();
  void HandleInput();
  void SelectedGroupAction(std::function<void(InstrumentInstance*, uint32, uint32)> action);
  void NewSong();
  void LoadSong();
  void SaveSong();
  std::string GetUniqueInstrumentInstanceName(std::string instrumentInstanceNameBase);
public:
  SongTab(ComposerView* composerView);
  ~SongTab();

  void Show() override;
  void Hide() override;
  void DoMainMenuBar() override;
  void Render(ImVec2 canvasSize) override;
  void DoLockedActions() override;
};
