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
  class SoundInformation {
  public:
    using SoundFactoryFunction = std::function<Sound* (const ReadSerializer& serializer)>;

    std::string name;
    std::string desc;
    std::string dialog;
    SoundFactoryFunction soundFactory;

    inline SoundInformation() {

    }

    inline SoundInformation(std::string name, std::string desc, std::string dialog, SoundFactoryFunction soundFactory)
      : name(name)
      , desc(desc)
      , dialog(dialog)
      , soundFactory(soundFactory) {
      SoundFactory::Register(*this);
    }
  };

private:
  static std::map<std::string, SoundInformation> soundInfoMap;
public:
  SoundFactory() = delete;

  static bool Register(const SoundInformation& soundInfo) {
    auto mapEntry = soundInfoMap.find(soundInfo.name);
    if (mapEntry != soundInfoMap.end()) {
      return false;
    }
    soundInfoMap[soundInfo.name] = soundInfo;
    return true;
  }
  static const std::map<std::string, SoundInformation>& GetInfoMap() {
    return soundInfoMap;
  }
};

#define REGISTER_SOUND(SoundClass, SoundDesc, DialogClass) \
  SoundFactory::SoundInformation SoundClass##FactoryInfo(#SoundClass, \
    SoundDesc, \
    #DialogClass, \
    [](const ReadSerializer& serializer) { \
      return new SoundClass(serializer); \
    })

