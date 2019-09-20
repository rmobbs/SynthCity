#pragma once

#include "ImGuiRenderable.h"
#include <map>
#include <typeinfo>

class View {
protected:
public:
  View() {
  }

  virtual ~View() {
  }

  virtual void Show() {

  }

  virtual void Hide() {

  }

  // Called from the high-precision audio callback
  virtual void OnAudioCallback(float beatTime) {

  }

  // Called for every beat when a song is loaded and the sequencer is playing. Note that the
  // beat index can go past the end of the song; this allows clients to determine their own
  // intro and looping behavior
  virtual void OnBeat(uint32 beatIndex) {

  }

  virtual void Render(ImVec2 canvasSize) = 0;







  // View manager stuff
  static std::map<uint32, View*> viewsByClassHash;
  static uint32 currentViewHash;

  static void Term() {
    for (auto& view : viewsByClassHash) {
      delete view.second;
    }
    viewsByClassHash.clear();
  }

  template<typename T> static void RegisterView(View* view) {
    viewsByClassHash.insert({ typeid(T).hash_code(), view });
  }

  template<typename T> static View* GetView() {
    auto viewByClassHash = viewsByClassHash.find(typeid(T).hash_code());
    if (viewByClassHash != viewsByClassHash.end()) {
      return viewByClassHash->second;
    }
    return nullptr;
  }

  static View* GetCurrentView() {
    auto viewByClassHash = viewsByClassHash.find(currentViewHash);
    if (viewByClassHash != viewsByClassHash.end()) {
      return viewByClassHash->second;
    }
    return nullptr;
  }

  template<typename T> static void SetCurrentView() {
    auto viewByClassHash = viewsByClassHash.find(currentViewHash);
    if (viewByClassHash != viewsByClassHash.end()) {
      viewByClassHash->second->Hide();
    }

    currentViewHash = typeid(T).hash_code();

    viewByClassHash = viewsByClassHash.find(currentViewHash);
    if (viewByClassHash != viewsByClassHash.end()) {
      viewByClassHash->second->Show();
    }
  }

  static void SetCurrentView(uint32 viewHash) {
    currentViewHash = viewHash;
  }
};
