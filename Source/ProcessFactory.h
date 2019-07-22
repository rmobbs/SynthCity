#pragma once

#include "Factory.h"

class Process;
class ProcessFactory : public Factory<Process> {

};

#define REGISTER_PROCESS(ProcessClass, ProcessDesc) FACTORY_REGISTER(ProcessFactory, ProcessClass, ProcessDesc)
