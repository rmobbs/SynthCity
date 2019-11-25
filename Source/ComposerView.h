#pragma once

#include "View.h"
#include "ImGuiRenderable.h"
#include "Sequencer.h"

#include "GL/glew.h"
#include "glm/vec4.hpp"

#include <string>
#include <vector>
#include <functional>
#include <set>
#include <atomic>

class Dialog;
class InstrumentInstance;
class Instrument;

class ComposerView : public View, public Sequencer::Listener {
protected:
  enum class DialogState {
    NoDialog,
    Pending,
    Active,
    Finish,
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

  HashedController<View> tabController;

  std::pair<DialogState, Dialog*> dialogState = { DialogState::NoDialog, nullptr };

  bool wasPlaying = false;

  uint32 logResponderId = UINT32_MAX;
  bool wasConsoleOpen = true;
  ImGuiRenderable renderable;
  bool localGuiDisabled = false;
  std::pair<InstrumentInstance*, int32> soloTrackInstance = { nullptr, -1 };
  std::atomic<bool> isMetronomeOn = false;
  std::atomic<bool> isLooping = false;

  void ConditionalEnableBegin(bool condition);
  void ConditionalEnableEnd();
  
  void InitResources();
  void NotePlayedCallback(uint32 trackIndex, uint32 noteIndex);

public:
  ComposerView(HashedController<View>* viewController);
  ~ComposerView();

  void Render(ImVec2 canvasSize) override;

  void Show() override;
  void Hide() override;

  void OnBeat(uint32 beatIndex) override;
  void HandleInput() override;
  void DoLockedActions() override;
  void SetTrackColors(Instrument* instrument, std::string paletteEntryName);
  void ShowDialog(Dialog* dialog);

  inline void SetSoloTrackInstance(InstrumentInstance* instrumentInstance, int32 trackId) {
    soloTrackInstance = { instrumentInstance, trackId };
  }

  inline const auto& GetSoloTrackInstance() const {
    return soloTrackInstance;
  }

  inline bool IsMetronomeOn() const {
    return isMetronomeOn;
  }

  inline void SetMetronomeOn(bool isOn) {
    isMetronomeOn = isOn;
  }

  inline bool IsLooping() const {
    return isLooping;
  }

  inline void SetLooping(bool isOn) {
    isLooping = isOn;
  }
};
