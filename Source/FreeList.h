#pragma once

#include <list>
#include <vector>
#include <functional>

template<class ClassName, typename... Args> class StaticFreeList {
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
    for (uint32 i = 0; i < startCount; ++i) {
      auto item = new ClassName;
      ItemFree().push_back(item);
      ItemHeap().push_back(item);
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
    if (ItemFree().empty()) {
      auto item = new ClassName(args...);
      ItemFree().push_back(item);
      ItemHeap().push_back(item);
    }
    auto item = new (ItemFree().front()) ClassName(args...);
    ItemFree().pop_front();
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

template<class ClassName, typename... Args> class FreeList {
private:
  std::list<ClassName*> itemFree;
  std::vector<ClassName*> itemHeap;
public:
  FreeList() = default;

  FreeList(uint32 startCount) {
    Init(startCount);
  }

  ~FreeList() {
    Term();
  }

  void Init(uint32 startCount) {
    for (uint32 i = 0; i < startCount; ++i) {
      auto item = new ClassName;
      itemFree.push_back(item);
      itemHeap.push_back(item);
    }
  }

  void Term() {
    itemFree.clear();
    for (auto& item : itemHeap) {
      delete item;
    }
    itemHeap.clear();
  }

  ClassName* Borrow(Args... args) {
    if (itemFree.empty()) {
      auto item = new ClassName;
      itemHeap.push_back(item);
      itemFree.push_back(item);
    }
    auto item = new (itemFree.front()) ClassName(args...);
    itemFree.pop_front();
    return item;
  }

  void Return(ClassName* item) {
    itemFree.push_back(item);
  }

  void ReturnAll() {
    itemFree.clear();
    for (auto& item : itemHeap) {
      itemFree.push_back(item);
    }
  }
};

template<class ClassName, typename... Args> class FunctorFreeList {
public:
  using CreateConstructorType = std::function<ClassName* ()>;
  using BorrowConstructorType = std::function<ClassName* (void*, Args...)>;
private:
  CreateConstructorType createConstructor;
  BorrowConstructorType borrowConstructor;
  std::list<ClassName*> itemFree;
  std::vector<ClassName*> itemHeap;
public:
  FunctorFreeList() = default;

  FunctorFreeList(uint32 startCount, const CreateConstructorType& createConstructor, const BorrowConstructorType& borrowConstructor) {
    Init(startCount, createConstructor, borrowConstructor);
  }

  ~FunctorFreeList() {
    Term();
  }

  void Init(uint32 startCount, const CreateConstructorType& createConstructor, const BorrowConstructorType& borrowConstructor) {
    this->createConstructor = createConstructor;
    this->borrowConstructor = borrowConstructor;

    for (uint32 i = 0; i < startCount; ++i) {
      auto item = this->createConstructor();
      itemFree.push_back(item);
      itemHeap.push_back(item);
    }
  }

  void Term() {
    itemFree.clear();
    for (auto& item : itemHeap) {
      delete item;
    }
    itemHeap.clear();
  }

  ClassName* Borrow(Args... args) {
    if (itemFree.empty()) {
      auto item = createConstructor();
      itemFree.push_back(item);
      itemHeap.push_back(item);
    }
    auto item = borrowConstructor(itemFree.front(), args...);
    itemFree.pop_front();
    return item;
  }

  void Return(ClassName* item) {
    itemFree.push_back(item);
  }

  void ReturnAll() {
    itemFree.clear();
    for (auto& item : itemHeap) {
      itemFree.push_back(item);
    }
  }
};


template<typename KeyType, class ClassName, typename... Args> class MappedFunctorFreeList {
friend class Information;
public:
  static std::map<KeyType, FunctorFreeList<ClassName, Args...>>& FreeListMap() {
    static std::map<KeyType, FunctorFreeList<ClassName, Args...>> freeListMap;
    return freeListMap;
  }
public:
  class Information {
  public:
    Information(const KeyType& key, uint32 startCount,
      const typename FunctorFreeList<ClassName, Args...>::CreateConstructorType& createConstructor,
      const typename FunctorFreeList<ClassName, Args...>::BorrowConstructorType& borrowConstructor) {
      // Odd that doing this in one step crashes ...
      auto freeList = MappedFunctorFreeList<KeyType, ClassName, Args...>::FreeListMap().insert({ key, { } });
      freeList.first->second.Init(startCount, createConstructor, borrowConstructor);
    }
  };

  static FunctorFreeList<ClassName, Args...>& FreeList(const KeyType& key) {
    return FreeListMap().find(key)->second;
  }
};

