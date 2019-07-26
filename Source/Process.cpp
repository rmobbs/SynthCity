#include "Process.h"

ProcessInstance::ProcessInstance(Process* process, float patchDuration)
  : process(process)
  , patchDuration(patchDuration)
  , processHash(process->GetClassHash()) {

}

Process::Process(const Process& that)
  : className(that.className)
  , classHash(that.classHash) {

}

Process::Process(const std::string& className)
  : className(className)
  , classHash(std::hash<std::string>{}(className)) {

}

