#pragma once

#include <list>
#include <vector>

template<class ClassName, typename... Args> class StaticFreeList {
private:
  using BorrowConstructorType = std::function<ClassName* (void*, Args...)>;
  
  static BorrowConstructorType& BorrowConstructor() {
    static BorrowConstructorType borrowConstructor;
    return borrowConstructor;
  }

  static std::list<ClassName*>& ItemFree() {
    static std::list<ClassName*> itemFree;
    return itemFree;
  }

  static std::vector<ClassName*>& ItemHeap() {
    static std::vector<ClassName*> itemHeap;
    return itemHeap;
  }
public:

  static void Init(uint32 startCount, const BorrowConstructorType& borrowConstructor = nullptr) {
    ItemHeap().resize(startCount);
    for (uint32 i = 0; i < startCount; ++i) {
      ItemHeap()[i] = new ClassName;
    }
    if (borrowConstructor != nullptr) {
      BorrowConstructor() = borrowConstructor;
    }
    else {
      BorrowConstructor() = [](void* memory) { return new (memory) ClassName; };
    }
  }
  static void Term() {
    ItemFree().clear();
    for (auto& item : ItemHeap()) {
      delete item;
    }
    ItemHeap().clear();
  }
  static ClassName* Borrow(Args... args) {
    if (ItemFree().size() > 0) {
      auto item = BorrowConstructor()(ItemFree().front(), args...);
      ItemFree().pop_front();
      return item;
    }
    auto item = new ClassName(args...);
    ItemHeap().push_back(item);
    return item;
  }
  static void Return(ClassName* item) {
    ItemFree().push_back(item);
  }
  static void ReturnAll() {
    ItemFree().clear();
    for (auto& item : ItemHeap()) {
      ItemFree().push_back(item);
    }
  }
};

#if 0
template<class ClassName, typename... Args> class FreeListMapped {
public:
  static std::map<std::string, std::pair<StaticFreeList<ClassName>, std::function<ClassName*(Args...)>>>& FreeListMap() {
    static std::map<std::string, std::pair<StaticFreeList<ClassName>, std::function<ClassName*(Args...)>>> freeListMap;
    return freeListMap;
  }

  static void Register(std::string className, std::function<ClassName* (Args...)> factory) {
    FreeListMap().insert({ className, {}})
  }
};
#endif
