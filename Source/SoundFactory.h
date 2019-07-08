#pragma once

#include "BaseTypes.h"
#include "BaseWindows.h"
#include "SerializeFwd.h"

#include <string>
#include <map>
#include <functional>

class Sound;
class DialogPage;

// Factory for creating sounds and related objects
class SoundFactory {
public:
  class SoundInformation {
  public:
    using SoundFactoryFunction = std::function<Sound* (const ReadSerializer& serializer)>;
    using PageFactoryFunction = std::function<DialogPage* (HINSTANCE, HWND)>;

    std::string name;
    std::string desc;
    SoundFactoryFunction soundFactory;
    PageFactoryFunction pageFactory;

    inline SoundInformation() {

    }

    inline SoundInformation(std::string name, std::string desc, SoundFactoryFunction soundFactory, PageFactoryFunction pageFactory)
      : name(name)
      , desc(desc)
      , soundFactory(soundFactory)
      , pageFactory(pageFactory) {
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

#define REGISTER_SOUND(SoundClass, SoundDesc, PageClass) \
  SoundFactory::SoundInformation SoundClass##FactoryInfo(#SoundClass, \
    SoundDesc, \
    [](const ReadSerializer& serializer) { \
      return new SoundClass(serializer); \
    }, \
    [](HINSTANCE hInstance, HWND hWndParent) { \
      auto newPage = new PageClass(hInstance, hWndParent); \
      SendMessage(newPage->GetHandle(), PageClass::WM_USERINIT, 0, reinterpret_cast<LPARAM>(newPage)); \
      return newPage; \
    })

