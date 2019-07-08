#include "Logging.h"
#include <map>
#include <stdarg.h>

namespace Logging {
  uint32 nextResponderId = 0;
  std::map<uint32, std::function<void(const std::string_view&)>> respondersById;

  void McLog(Category category, const char *fmt, ...) {
    char buf[1024];
    buf[0] = static_cast<char>(category) + 1; // Don't let that zero get ya
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf + 1, _countof(buf) - 2, fmt, args);
    buf[_countof(buf) - 1] = 0;
    va_end(args);

    std::string_view dispatchBuf(buf);
    for (const auto& responderEntry : respondersById) {
      responderEntry.second(dispatchBuf);

      // TODO: Handle fatal
    }
  }

  uint32 AddResponder(const std::function<void(const std::string_view&)> &newResponder) {
    auto currResponderId = nextResponderId++;
    respondersById.insert_or_assign(currResponderId, newResponder);
    return currResponderId;
  }

  void PopResponder(uint32 responderId) {
    const auto& responderEntry = respondersById.find(responderId);
    if (responderEntry != respondersById.end()) {
      respondersById.erase(responderEntry);
    }
  }
}