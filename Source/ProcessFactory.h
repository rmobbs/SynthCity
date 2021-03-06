#pragma once

#include "Factory.h"
#include "FreeList.h"

using ProcessFactory = Factory<class Process>;
#define REGISTER_PROCESS(ProcessClass, ProcessDesc) FACTORY_REGISTER(ProcessFactory, ProcessClass, ProcessDesc)


using ProcessInstanceFreeList = MappedFunctorFreeList<uint32, class ProcessInstance, class Process*, float>;
#define REGISTER_PROCESS_INSTANCE(ProcessInstanceClass, ProcessClass, InitialSize) \
  ProcessInstanceFreeList::Information FreeList##ProcessInstanceClass(std::hash<std::string>{}(#ProcessClass), InitialSize, \
    []() { \
      return new ProcessInstanceClass; \
    }, \
    [](void* memory, Process* process, float patchDuration) { \
      return new (memory) ProcessInstanceClass(process, patchDuration); \
    });


