#include "OddsAndEnds.h"

std::shared_ptr<WCHAR[]> StringToWChar(const std::string& sourceString) {
  int bufferlen = ::MultiByteToWideChar(CP_ACP, 0, sourceString.c_str(), sourceString.length(), nullptr, 0);
  if (bufferlen > 0) {
    auto stringLen = sourceString.length();

    std::shared_ptr<WCHAR[]> stringBuf(new WCHAR[stringLen + 1]);
    ::MultiByteToWideChar(CP_ACP, 0, sourceString.c_str(), stringLen, stringBuf.get(), bufferlen);
    stringBuf.get()[stringLen] = 0;
    return stringBuf;
  }
  return nullptr;
}

std::shared_ptr<WCHAR[]> StringToWChar(const std::string_view& sourceString) {
  int bufferlen = ::MultiByteToWideChar(CP_ACP, 0, sourceString.data(), sourceString.length(), nullptr, 0);
  if (bufferlen > 0) {
    auto stringLen = sourceString.length();

    std::shared_ptr<WCHAR[]> stringBuf(new WCHAR[stringLen + 1]);
    ::MultiByteToWideChar(CP_ACP, 0, sourceString.data(), stringLen, stringBuf.get(), bufferlen);
    stringBuf.get()[stringLen] = 0;
    return stringBuf;
  }
  return nullptr;
}

bool iequals(const std::string& a, const std::string& b) {
  return std::equal(a.begin(), a.end(),
    b.begin(), b.end(),
    [](char a, char b) {
      return tolower(a) == tolower(b);
    });
}

