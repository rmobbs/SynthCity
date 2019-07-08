#pragma once

#include <type_traits>
template <typename Enum, Enum First, Enum LastVal> class EnumIterator {
private:
  typedef typename std::underlying_type<Enum>::type val_t;
  int val;
public:
  EnumIterator(const Enum& e) : val(static_cast<val_t>(e)) {}
  EnumIterator() : val(static_cast<val_t>(First)) {}
  EnumIterator operator++() {
    ++val;
    return *this;
  }
  Enum operator*() { return static_cast<Enum>(val); }
  EnumIterator begin() { return *this; }
  EnumIterator end() {
    static const EnumIterator endIter = ++EnumIterator(LastVal);
    return endIter;
  }
  bool operator!=(const EnumIterator& i) { return val != i.val; }
};

#if 0
template<typename T> class reverse_adapter { 
  public: 
    reverse_adapter(T& c) : c(c) {} 
    typename T::reverse_iterator begin() {
      return c.rbegin();
    }
    typename T::reverse_iterator end() {
      return c.rend();
    }
private:
  T& c;
};

template<typename T> reverse_adapter<T> reverse_adapt_container(T &c) { return reverse_adapter<T>(c); }
#endif
