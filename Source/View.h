#pragma once

#include "BaseTypes.h"
#include "HashedController.h"
#include "ImGuiRenderable.h"

class View {
protected:
  std::string name;
  HashedController<View>* viewController = nullptr;
public:
  View(std::string name, HashedController<View>* viewController = nullptr)
  : name(name)
  , viewController(viewController) {
  }

  virtual ~View() {
  }

  virtual void Show() {

  }

  virtual void Hide() {

  }

  virtual void DoLockedActions() {

  }

  virtual void DoMainMenuBar() {

  }

  virtual void Render(ImVec2 canvasSize) = 0;

  inline std::string GetName() const {
    return name;
  }
};
