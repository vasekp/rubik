#ifndef PERMUTATION_HPP
#define PERMUTATION_HPP

#include <initializer_list>
#include <vector>
#include <numeric>
#include <iterator>
#include <algorithm>
#include <utility>
#include <numeric>
#include <cassert>

class Permutation {
public:
  using entry_type = unsigned;
  using size_type = entry_type;
  using numbered_type = unsigned long long;
  std::vector<entry_type> cycles;

public:
  Permutation() : cycles{} { }

  Permutation(const std::initializer_list<entry_type>& list_)
    : Permutation(std::vector<entry_type>{list_})
  { }

  explicit Permutation(const std::vector<entry_type>& list) : cycles{} {
    size_type size = list.size();
    assert(size > 0);
    std::vector<bool> used(size);
    for(size_type start = 0u; start < size; start++) {
      if(used[start])
        continue;
      if(list[start] == start)
        continue;
      for(auto i = start; !used[i]; i = list[i]) {
        assert(i < size);
        cycles.push_back(i);
        used[i] = true;
      }
      cycles.push_back(start);
    }
  }

private:
  struct private_tag { };
  
  Permutation(std::vector<entry_type>&& cycles_, const private_tag&) : cycles(std::move(cycles_)) { }

public:
  static Permutation from_cycles(const std::initializer_list<std::vector<entry_type>>& cycles_) {
    decltype(cycles) cycles{};
    for(const auto& cycle : cycles_) {
      assert(cycle.size() > 0);
      if(cycle.size() == 1)
        continue;
      std::copy(cycle.begin(), cycle.end(), std::back_inserter(cycles));
      cycles.push_back(cycle.front());
    }
    return {std::move(cycles), private_tag{}};
  }

  static Permutation from_numbered(numbered_type ix) {
    Permutation ret{};
    for(size_type i = 2; ix > 0; i++) {
      auto rem = ix % i;
      ix /= i;
      if(rem == 0)
        continue;
      std::vector<entry_type> cycle(rem + 2);
      std::iota(cycle.begin(), cycle.end() - 1, i - rem - 1);
      cycle.back() = cycle.front();
      ret = Permutation{std::move(cycle), private_tag{}} * ret;
    }
    return ret;
  }

  static numbered_type to_numbered(Permutation p) {
    numbered_type ret{0};
    auto max = p._max();
    for(auto i = max; i >= 1; i--) {
      auto j = p[i];
      ret *= i + 1;
      ret += i - j;
      std::vector<entry_type> cycle(i - j + 2);
      std::iota(cycle.begin(), cycle.end() - 1, j);
      std::reverse(cycle.begin(), cycle.end() - 1);
      cycle.back() = cycle.front();
      p = Permutation{std::move(cycle), private_tag{}} * p;
    }
    return ret;
  }

  numbered_type to_numbered() const {
    return Permutation::to_numbered(*this);
  }

  std::vector<entry_type> to_list(size_type max) const {
    std::vector<entry_type> ret(max);
    std::iota(ret.begin(), ret.end(), 0);
    auto cyclestart = cycles.end();
    for(auto it = cycles.begin(); it != cycles.end(); it++) {
      if(cyclestart == cycles.end())
        cyclestart = it;
      else if(*cyclestart == *it) {
        cyclestart = cycles.end();
        continue;
      }
      assert(cyclestart + 1 != cycles.end());
      ret[*it] = *(it + 1);
    }
    return ret;
  }

  std::vector<entry_type> to_list() const {
    return to_list(_max() + 1);
  }

  static std::vector<entry_type> to_list(const Permutation& p) {
    return p.to_list();
  }

  std::vector<std::vector<entry_type>> to_cycles() const {
    std::vector<std::vector<entry_type>> ret{};
    for(auto it = cycles.begin(); it != cycles.end(); it++) {
      auto it2 = std::find(it + 1, cycles.end(), *it);
      std::vector<entry_type> cycle{};
      std::copy(it, it2, std::back_inserter(cycle));
      ret.push_back(std::move(cycle));
      it = it2;
    }
    return ret;
  }

  static std::vector<std::vector<entry_type>> to_cycles(const Permutation& p) {
    return p.to_cycles();
  }

  entry_type operator[](entry_type ix) const {
    auto it = std::find(cycles.begin(), cycles.end(), ix);
    if(it == cycles.end())
      return ix;
    else
      return *(it + 1);
  }

  friend Permutation operator*(const Permutation& p1, const Permutation& p2) {
    std::vector<entry_type> ncycles{};
    auto max = std::max(p1._max(), p2._max());
    std::vector<bool> used(max);
    for(auto start = 0u; start < max; start++) {
      if(used[start])
        continue;
      used[start] = true;
      auto next = p1[p2[start]];
      if(next == start)
        continue;
      ncycles.push_back(start);
      for(auto i = next; i != start; i = p1[p2[i]]) {
        ncycles.push_back(i);
        used[i] = true;
      }
      ncycles.push_back(start);
    }
    return {std::move(ncycles), private_tag{}};
  }

  Permutation& operator*=(const Permutation& other) {
    auto result = (*this) * other;
    std::swap(*this, result);
    return *this;
  }

  friend bool operator==(const Permutation& p1, const Permutation& p2) {
    // quick check
    if(p1.cycles.size() != p2.cycles.size())
      return false;
    auto n = p1._max();
    for(auto i = 0u; i < n; i++)
      if(p1[i] != p2[i])
        return false;
    // else
    return true;
  }

  friend bool operator!=(const Permutation& p1, const Permutation& p2) {
    return !(p1 == p2);
  }

  Permutation inverse() const {
    std::vector<entry_type> ncycles = cycles;
    for(auto it = ncycles.begin(); it != ncycles.end(); it++) {
      auto it2 = std::find(it + 1, ncycles.end(), *it);
      std::reverse(it + 1, it2);
      it = it2;
    }
    return {std::move(ncycles), private_tag{}};
  }

  friend Permutation inverse(const Permutation& p) {
    return p.inverse();
  }

  int sign() const {
    return cycles.size() % 2 ? -1 : 1;
  }

  size_type order() const {
    size_type ret = 1;
    for(auto it = cycles.begin(); it != cycles.end(); it++) {
      auto it2 = std::find(it + 1, cycles.end(), *it);
      ret = std::lcm(ret, std::distance(it2, it));
      it = it2;
    }
    return ret;
  }

private:
  entry_type _max() const {
    if(cycles.size() == 0)
      return 0;
    else
      return *std::max_element(cycles.begin(), cycles.end());
  }

};

namespace std {
  template<>
  struct hash<Permutation> {
    using argument_type = Permutation;
    using result_type = Permutation::numbered_type;
    result_type operator() (const Permutation& p) const {
      return Permutation::to_numbered(p);
    }
  };
}

#endif
