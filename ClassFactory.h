#pragma once

#include <map>
#include <string>
#include <functional>
#include "BaseTypes.h"
#include "SerializeFwd.h"

template<class FactoryClass> class ClassFactory {
public:
  class ClassInformation {
  public:
    using FactoryFunction = std::function<FactoryClass* (const ReadSerializer& serializer)>;

    uint32 tag;
    std::string name;
    std::string desc;

    FactoryFunction createSerialized;

    inline ClassInformation() {

    }

    inline ClassInformation(uint32 tag, std::string name, std::string desc, FactoryFunction createSerialized) 
    : tag(tag)
    , name(name)
    , desc(desc)
    , createSerialized(createSerialized) {
      ClassFactory::Register(*this);
    }
  };

protected:
  static std::map<std::string, ClassInformation> classInfoMap;
public:
  ClassFactory() = delete;

  static bool Register(const ClassInformation& classInfo) {
    auto mapEntry = classInfoMap.find(classInfo.name);
    if (mapEntry != classInfoMap.end()) {
      return false;
    }
    classInfoMap[classInfo.name] = classInfo;
    return true;
  }
  static const std::map<std::string, ClassInformation>& GetInfoMap() {
    return classInfoMap;
  }
};

template<class FactoryClass> std::map<std::string, typename 
  ClassFactory<FactoryClass>::ClassInformation> ClassFactory<FactoryClass>::classInfoMap;

#define DECLARE_CLASS_FACTORY(ClassFactoryName, ClassName) \
  class ClassFactoryName : public ClassFactory<ClassName> {};
#define DECLARE_FACTORY_CLASS(ClassName) \
  static constexpr const char* kClassName = #ClassName; \
  virtual const char*GetClassName() const { \
    return kClassName; \
  }

#define REGISTER_FACTORY_CLASS(ClassFactoryName, ClassName, ClassType, ClassDesc) \
  ClassFactoryName::ClassInformation ClassName##ClassInfo(ClassType,\
    ClassName::kClassName,\
    ClassDesc,\
    [](const ReadSerializer& serializer) {\
      return new ClassName(serializer);\
    })

