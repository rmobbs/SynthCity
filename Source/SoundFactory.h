#pragma once

#include "BaseTypes.h"
#include "BaseWindows.h"
#include "SerializeFwd.h"

#include <string>
#include <map>
#include <functional>

class Sound;

// Factory for creating sounds and related objects
class SoundFactory {
public:
  class Information {
  public:
    using FactoryFunction = std::function<Sound* (const ReadSerializer& serializer)>;

    std::string name;
    std::string desc;
    std::string dialog;
    FactoryFunction factory;

    inline Information() {

    }

    inline Information(std::string name, std::string desc, std::string dialog, FactoryFunction factory)
      : name(name)
      , desc(desc)
      , dialog(dialog)
      , factory(factory) {
      SoundFactory::Register(*this);
    }
  };

private:
  // Static initialization order fiasco
  static std::map<std::string, Information>& InfoMap() {
    static std::map<std::string, Information> infoMap;
    return infoMap;
  }
public:
  SoundFactory() = delete;

  static bool Register(const Information& info) {
    auto& infoMap = InfoMap();

    auto mapEntry = infoMap.find(info.name);
    if (mapEntry != infoMap.end()) {
      return false;
    }
    infoMap[info.name] = info;
    return true;
  }
  static const std::map<std::string, Information>& GetInfoMap() {
    return InfoMap();
  }
};

#define REGISTER_SOUND(SoundClass, SoundDesc, DialogClass) \
  SoundFactory::Information SoundClass##FactoryInfo(#SoundClass, \
    SoundDesc, \
    #DialogClass, \
    [](const ReadSerializer& serializer) { \
      return new SoundClass(serializer); \
    })

