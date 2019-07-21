#pragma once

#include "BaseTypes.h"
#include "BaseWindows.h"
#include "SerializeFwd.h"

#include <string>
#include <map>
#include <functional>

class Process;

// Factory for creating processes and related objects
class ProcessFactory {
public:
  class Information {
  public:
    using FactoryFunction = std::function<Process* (const ReadSerializer& serializer)>;

    std::string name;
    std::string desc;
    FactoryFunction factory;

    inline Information() {

    }

    inline Information(std::string name, std::string desc, FactoryFunction factory)
      : name(name)
      , desc(desc)
      , factory(factory) {
      ProcessFactory::Register(*this);
    }
  };

  // Static initialization order fiasco
  static std::map<std::string, Information>& InfoMap() {
    static std::map<std::string, Information> infoMap;
    return infoMap;
  }

public:
  ProcessFactory() = delete;

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

#define REGISTER_PROCESS(ProcessClass, ProcessDesc) \
  ProcessFactory::Information ProcessClass##FactoryInfo(#ProcessClass, \
    ProcessDesc, \
    [](const ReadSerializer& serializer) { \
      return new ProcessClass(serializer); \
    });

