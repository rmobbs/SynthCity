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

  struct InstrumentTrackBeat {
    Song::InstrumentInstance* instrumentInstance = nullptr;
    uint32 trackId = kInvalidUint32;
    uint32 beatIndex = kInvalidUint32;

    inline bool operator==(const InstrumentTrackBeat& that) {
      return instrumentInstance == that.instrumentInstance && trackId == that.trackId && beatIndex == that.beatIndex;
    }
    inline bool operator!=(const InstrumentTrackBeat& that) {
      return !operator==(that);
    }
  };

  struct InstrumentTrack {
    Song::InstrumentInstance* instrumentInstance = nullptr;
    uint32 trackId = kInvalidUint32;

    inline bool operator==(const InstrumentTrack& that) {
      return instrumentInstance == that.instrumentInstance && trackId == that.trackId;
    }
    inline bool operator!=(const InstrumentTrack& that) {
      return !operator==(that);
    }
  };

  struct TrackBeat {
    uint32 trackId = kInvalidUint32;
    uint32 beatIndex = kInvalidUint32;

    inline bool operator==(const TrackBeat& that) {
      return trackId == that.trackId && beatIndex == that.beatIndex;
    }
    inline bool operator!=(const TrackBeat& that) {
      return !operator==(that);
    }
  };

  struct SongTrack {
    struct Note {
      Song::Note* note = nullptr;
      std::string uniqueGuiId;
    };

    std::string uniqueGuiIdHamburgerMenu;
    std::string uniqueGuiIdPropertiesPop;

    uint32 trackId = kInvalidUint32;
    std::vector<Note> notes;
    bool mute = false;
  };

  struct InstrumentInstanceData {
    Song::InstrumentInstance* instrumentInstance = nullptr;
    std::map<uint32, SongTrack> songTracks;
    std::string uniqueGuiIdName;
    std::string uniqueGuiIdHamburgerMenu;
    std::string uniqueGuiIdPropertiesPop;
  };

  bool wasPlaying = false;

  InstrumentTrack pendingPlayTrack;
  InstrumentTrack pendingSoloTrack;
  InstrumentTrack pendingCloneTrack;
  InstrumentTrack pendingRemoveTrack;
  struct { Song::InstrumentInstance* i; int32 d; } pendingInstrumentMove = { nullptr, 0 };
  Song::InstrumentInstance* pendingRemoveInstrumentInstance = nullptr;
  int32 pendingSubdivision = kInvalidUint32;
  uint32 pendingTempo = kInvalidUint32;
  uint32 pendingAddMeasures = kInvalidUint32;
  bool pendingNewInstrument = false;
  bool pendingLoadInstrument = false;
  Song::InstrumentInstance* pendingSaveInstrument = nullptr;
  bool pendingNewSong = false;
  bool pendingLoadSong = false;
  bool pendingSaveSong = false;

  std::map<Song::InstrumentInstance*, std::map<uint32, std::set<uint32>>> selectedNotesByInstrument;
  std::map<Song::InstrumentInstance*, std::map<uint32, std::set<uint32>>> selectingNotesByInstrument;

  std::map<Song::InstrumentInstance*, InstrumentInstanceData> instrumentInstanceDataMap;
  std::list<InstrumentInstanceData*> orderedInstrumentInstanceData;
  glm::vec4 dragBox = { -1.0f, -1.0f, -1.0f, -1.0f };
  InstrumentTrackBeat toggledNote;
  InstrumentTrackBeat hoveredNote;
  struct { Song::InstrumentInstance* i; uint32 t; float v; } pendingTrackVolume = { nullptr, kInvalidUint32, 0.0f };
  struct { Song::InstrumentInstance* i; uint32 t; bool m; } pendingMuteTrack = { nullptr, kInvalidUint32, false };
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
  InstrumentTrack soloTrack;

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
  void SelectedGroupAction(std::function<void(Song::InstrumentInstance*, uint32, uint32)> action);
  void NewInstrument();
  Instrument* LoadInstrument(std::string requiredInstrument);
  void SaveInstrument(Instrument* instrument);
  void NewSong();
  void LoadSong();
  void SaveSong();
  std::string GetUniqueInstrumentName(std::string instrumentNameBase);
  std::string GetUniqueTrackName(Instrument* instrument, std::string trackNameBase);
  void OnSongTrackAdded(Song::InstrumentInstance* instrument, uint32 trackId);
  void OnSongTrackRemoved(Song::InstrumentInstance* instrument, uint32 trackId);
  void RebuildInstrumentInstanceDataMap(Song* song);

public:
  ComposerView(uint32 mainWindowHandle);
  ~ComposerView();

  void Render(ImVec2 canvasSize) override;

  void Show() override;
  void Hide() override;

  void OnBeat(uint32 beatIndex) override;
};
