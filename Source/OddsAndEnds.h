#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
#include <memory>
#include <string>
#include <set>
#include <map>
#include <stack>

std::shared_ptr<WCHAR[]> StringToWChar(const std::string& sourceString);
std::shared_ptr<WCHAR[]> StringToWChar(const std::string_view& sourceString);
bool iequals(const std::string& a, const std::string& b);

template<typename T, typename K> inline bool contains(const T& collection, const K& value) {
  return collection.find(value) != collection.end();
}

template<typename T> inline bool icontains(const T& collection, const std::string& value) {
  if (collection.empty()) {
    return false;
  }

  auto iter = collection.begin();
  while (iter != collection.end()) {
    if (iequals(iter->first, value)) {
      break;
    }
    ++iter;
  }
  return iter != collection.end();
}

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

template<typename K, typename V, typename T> inline void mapped_set_toggle(std::map<K, std::set<V>>& theMap, const K& theKey, const T& theValue) {
  auto mapEntry = theMap.find(theKey);
  if (mapEntry != theMap.end()) {
    set_toggle(mapEntry->second, theValue);
  }
  else {
    theMap.insert({ theKey, { theValue } });
  }
}

template<typename K, typename V, typename T> inline void mapped_set_add(std::map<K, std::set<V>>& theMap, const K& theKey, const T& theValue) {
  auto mapEntry = theMap.find(theKey);
  if (mapEntry != theMap.end()) {
    set_add(mapEntry->second, theValue);
  }
  else {
    theMap.insert({ theKey, { theValue } });
  }
}

template<typename K, typename V, typename T> inline void mapped_set_remove(std::map<K, std::set<V>>& theMap, const K& theKey, const T& theValue) {
  auto mapEntry = theMap.find(theKey);
  if (mapEntry != theMap.end()) {
    set_remove(mapEntry->second, theValue);
  }
}

template<typename K, typename V, typename T> inline bool mapped_set_contains(std::map<K, std::set<V>>& theMap, const K& theKey, const T& theValue) {
  auto mapEntry = theMap.find(theKey);
  if (mapEntry != theMap.end()) {
    return set_contains(mapEntry->second, theValue);
  }
  return false;
}

template <size_t BufferSize> class UniqueIdBuilder {
protected:
  char buffer[BufferSize] = { 0 };
  std::stack<size_t> offset;
public:
  inline UniqueIdBuilder(const char* prefix = nullptr) {
    if (prefix != nullptr) {
      strcpy(buffer, prefix);
      offset.push(strlen(prefix));
    }
    else {
      offset.push(0);
    }
  }

  inline void PushHex(size_t hexValue) {
    offset.push(offset.top() + strlen(_itoa(hexValue, buffer + offset.top(), 16)));
  }
  inline void PushUnsigned(size_t unsignedValue) {
    offset.push(offset.top() + strlen(_itoa(unsignedValue, buffer + offset.top(), 10)));
  }
  inline void Pop() {
    offset.pop();
    buffer[offset.top()] = 0;
  }

  const char* operator()() const {
    return buffer;
  }
};