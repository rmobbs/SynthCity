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
  class ProcessInformation {
  public:
    using FactoryFunction = std::function<Process* (const ReadSerializer& serializer)>;

    std::string name;
    std::string desc;
    std::string dialog;
    FactoryFunction factory;

    inline ProcessInformation() {

    }

    inline ProcessInformation(std::string name, std::string desc, std::string dialog, FactoryFunction factory)
      : name(name)
      , desc(desc)
      , dialog(dialog)
      , factory(factory) {
      ProcessFactory::Register(*this);
    }
  };

  // Static initialization order fiasco
  static std::map<std::string, ProcessInformation>& InfoMap() {
    static std::map<std::string, ProcessInformation> infoMap;
    return infoMap;
  }

public:
  ProcessFactory() = delete;

  static bool Register(const ProcessInformation& info) {
    auto& infoMap = InfoMap();

    auto mapEntry = infoMap.find(info.name);
    if (mapEntry != infoMap.end()) {
      return false;
    }
    infoMap[info.name] = info;
    return true;
  }

  static const std::map<std::string, ProcessInformation>& GetInfoMap() {
    return InfoMap();
  }
};

#define REGISTER_PROCESS(ProcessClass, ProcessDesc, DialogClass) \
  ProcessFactory::ProcessInformation ProcessClass##FactoryInfo(#ProcessClass, \
    ProcessDesc, \
    #DialogClass, \
    [](const ReadSerializer& serializer) { \
      return new ProcessClass(serializer); \
    })

