#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"

#include <string>
#include <map>
#include <functional>

template<class FactoryClass> class Factory {
  using FactoryFunction = std::function<FactoryClass* (const ReadSerializer& serializer)>;
public:
  class Information {
  public:

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
      Factory::Register(*this);
    }
  };

  // Static initialization order fiasco
  static std::map<std::string, Information>& InfoMap() {
    static std::map<std::string, Information> infoMap;
    return infoMap;
  }

public:
  Factory() = delete;

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

#define FACTORY_REGISTER(FactoryName, FactoryClass, ClassDesc, DialogClass) \
  FactoryName::Information FactoryClass##FactoryInfo(#FactoryClass, \
    ClassDesc, \
    #DialogClass, \
    [](const ReadSerializer& serializer) { \
      return new FactoryClass(serializer); \
    })

