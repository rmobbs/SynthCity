#pragma once

#include <map>
#include <string>

template<class ClassInformation> class ClassFactory {
protected:
  static std::map<std::string, ClassInformation> infoMap;
public:
  ClassFactory() = delete;

  static bool Register(const ClassInformation& classInfo) {
    auto mapEntry = infoMap.find(classInfo.name);
    if (mapEntry != infoMap.end()) {
      return false;
    }
    infoMap[classInfo.name] = classInfo;
    return true;
  }
  static const std::map<std::string, ClassInformation>& GetInfoMap() {
    return infoMap;
  }
};
template<class ClassInformation> std::map<std::string, ClassInformation> ClassFactory<ClassInformation>::infoMap;

#define DECLARE_CLASS_FACTORY(FactoryName, ClassInfoType) \
  class FactoryName : public ClassFactory<ClassInfoType> {};
#define DECLARE_CLASS_INFO static bool isRegistered; std::string name;
#define FACTORY_REGISTER(FactoryName, ClassInfoType, ClassInfoInstance) \
  bool ClassInfoType::isRegistered = FactoryName::Register(ClassInfoInstance)

