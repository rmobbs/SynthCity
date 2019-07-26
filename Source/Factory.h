#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"

#include <string>
#include <map>
#include <functional>

template<class FactoryClass> class Factory {
  using SerializeFunction = std::function<FactoryClass* (const ReadSerializer& serializer)>;
  using SpawnFunction = std::function<FactoryClass*()>;
public:
  class Information {
  public:

    std::string name;
    std::string desc;
    SerializeFunction serialize;
    SpawnFunction spawn;

    inline Information() {

    }

    inline Information(std::string name, std::string desc, SerializeFunction serialize, SpawnFunction spawn)
      : name(name)
      , desc(desc)
      , serialize(serialize)
      , spawn(spawn) {
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

#define FACTORY_REGISTER(FactoryName, FactoryClass, ClassDesc) \
  FactoryName::Information FactoryClass##FactoryInfo(#FactoryClass, \
    ClassDesc, \
    [](const ReadSerializer& serializer) { \
      return new FactoryClass(serializer); \
    }, \
    []() { \
      return new FactoryClass; \
    })

