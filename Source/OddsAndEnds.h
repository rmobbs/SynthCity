#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>
#include <string>
#include <set>

std::shared_ptr<WCHAR[]> StringToWChar(const std::string& sourceString);
std::shared_ptr<WCHAR[]> StringToWChar(const std::string_view& sourceString);
bool iequals(const std::string& a, const std::string& b);

template<typename T> inline bool set_contains(const std::set<T>& theSet, const T& theValue) {
  return (theSet.find(theValue) != theSet.end());
}

template<typename T> inline void set_toggle(std::set<T>& theSet, const T& theValue) {
  auto setEntry = theSet.find(theValue);
  if (setEntry != theSet.end()) {
    theSet.erase(setEntry);
  }
  else {
    theSet.insert(theValue);
  }
}

template<typename T> inline void set_add(std::set<T>& theSet, const T& theValue) {
  auto setEntry = theSet.find(theValue);
  if (setEntry == theSet.end()) {
    theSet.insert(theValue);
  }
}

template<typename T> inline void set_remove(std::set<T>& theSet, const T& theValue) {
  auto setEntry = theSet.find(theValue);
  if (setEntry != theSet.end()) {
    theSet.erase(setEntry);
  }
}

inline void ensure_fileext(std::string& fileName, std::string_view fileTag) {
  if (fileName.compare(fileName.length() - fileTag.length(), fileTag.length(), fileTag)) {
    fileName += fileTag;
  }
}

inline bool check_fileext(std::string fileName, std::string_view fileTag) {
  return fileName.compare(fileName.length() -
    fileTag.length(), fileTag.length(), fileTag) == 0;
}

