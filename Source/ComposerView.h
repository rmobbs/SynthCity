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

class ComposerView : public View, public Sequencer::Listener {
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

  HashedController<View> tabController;

  Dialog* pendingDialog = nullptr;
  Dialog* activeDialog = nullptr;

  bool wasPlaying = false;

  uint32 logResponderId = UINT32_MAX;
  bool wasConsoleOpen = true;
  ImGuiRenderable renderable;
  bool localGuiDisabled = false;
  std::pair<InstrumentInstance*, int32> soloTrackInstance = { nullptr, -1 };

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
  void DoLockedActions() override;

  inline void SetSoloTrackInstance(InstrumentInstance* instrumentInstance, int32 trackId) {
    soloTrackInstance = { instrumentInstance, trackId };
  }
  inline const auto& GetSoloTrackInstance() const {
    return soloTrackInstance;
  }
};
