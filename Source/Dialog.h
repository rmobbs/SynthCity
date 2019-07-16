#pragma once

#include "BaseTypes.h"
#include "BaseWindows.h"
#include "SerializeFwd.h"
#include "glm/vec2.hpp"
#include <map>
#include <functional>
#include <string>

class Dialog {
protected:
  bool exitedOk = false;
public:
  Dialog() = default;
  virtual ~Dialog() = default;

  virtual void Open();
  virtual bool Render();

  inline bool ExitedOk() const {
    return exitedOk;
  }

  virtual bool SerializeWrite(const WriteSerializer& serializer) = 0;
  virtual bool SerializeRead(const ReadSerializer& serializer) = 0;
};

class DialogFactory {
public:
  class DialogInformation {
  public:
    using DialogFactoryFunction = std::function<Dialog* ()>;

    DialogFactoryFunction factoryFunction;

    inline DialogInformation() {

    }

    inline DialogInformation(std::string name, DialogFactoryFunction factoryFunction)
      : factoryFunction(factoryFunction) {
      DialogFactory::Register(name, *this);
    }
  };

private:
  static std::map<std::string, DialogInformation> infoMap;
public:
  DialogFactory() = delete;

  static bool Register(std::string name, const DialogInformation& info) {
    auto mapEntry = infoMap.find(name);
    if (mapEntry != infoMap.end()) {
      return false;
    }
    infoMap[name] = info;
    return true;
  }
  static const std::map<std::string, DialogInformation>& GetInfoMap() {
    return infoMap;
  }
};

#define REGISTER_DIALOG(DialogClass) \
  DialogFactory::DialogInformation DialogClass##FactoryInfo(#DialogClass, \
    []() { \
      return new DialogClass; \
    })
