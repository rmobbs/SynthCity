#pragma once

#include <list>
#include <vector>

template<class ClassName> class FreeList {
private:
  static std::list<ClassName*>& ItemFree() {
    static std::list<ClassName*> itemFree;
    return itemFree;
  }

  static std::vector<ClassName*>& ItemHeap() {
    static std::vector<ClassName*> itemHeap;
    return itemHeap;
  }
public:

  static void Init(uint32 startCount) {
    ItemHeap().resize(startCount);
    for (uint32 i = 0; i < startCount; ++i) {
      ItemHeap()[i] = new ClassName;
    }
  }
  static void Term() {
    ItemFree().clear();
    for (auto& item : ItemHeap()) {
      delete item;
    }
    ItemHeap().clear();
  }
  static ClassName* Borrow() {
    if (ItemFree().size() > 0) {
      auto item = ItemFree().front();
      ItemFree().pop_front();
      return item;
    }
    auto item = new ClassName;
    ItemHeap().push_back(item);
    return item;
  }
  static void Return(ClassName* item) {
    ItemFree().push_back(item);
  }
};