#pragma once

#include "BaseTypes.h"
#include <functional>
#include <string_view>

namespace Logging {
  enum Category {
    Info,
    Warn,
    Error,
    Fatal,
    Count,
  };
  void McLog(Category category, const char *fmt, ...);
  uint32 AddResponder(const std::function<void(const std::string_view&)>&);
  void PopResponder(uint32 responderId);
}; // namespace Logging

#define MCLOG(category, fmt, ...) Logging::McLog(Logging::Category::category, fmt, __VA_ARGS__);