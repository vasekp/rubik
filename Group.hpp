#ifndef GROUP_HPP
#define GROUP_HPP

#include <vector>
#include <unordered_set>
#include <utility>
#include <iterator>
#include <algorithm>
#include <cassert>

template<class Group, class Matrix>
class Representation;

template<class Element>
class Group {
  std::vector<Element> gens;
  std::vector<Element> elems;
  std::vector<std::vector<std::size_t>> cayley;

public:
  using element_type = Element;

  Group(std::vector<Element> gens_)
    : gens(std::move(gens_)), elems{Element{}}, cayley{}
  {
    for(std::size_t i = 0; i < elems.size(); i++) {
      const auto p = elems[i];
      std::vector<std::size_t> c{};
      for(const auto& g : gens) {
        auto p1 = g*p;
        bool found = false;
        for(std::size_t j = 0; j < elems.size(); j++)
          if(elems[j] == p1) {
            c.push_back(j);
            found = true;
            break;
          }
        if(!found) {
          c.push_back(elems.size());
          elems.push_back(std::move(p1));
        }
      }
      cayley.push_back(std::move(c));
    }
  }

  template<class Group, class Matrix>
  friend class Representation;

  auto begin() const {
    return elems.cbegin();
  }

  auto end() const {
    return elems.cend();
  }

  auto size() const {
    return elems.size();
  }

  std::vector<std::vector<Element>> cosets_r(const Group<Element>& subgroup) const {
    std::vector<std::vector<Element>> ret{};
    std::unordered_set<Element> rest{};
    std::copy(elems.begin(), elems.end(), std::inserter(rest, rest.begin()));
    while(rest.size() > 0) {
      std::vector<Element> coset{};
      auto base = *rest.begin();
      for(const auto& elm : subgroup) {
        auto elm1 = base*elm;
        rest.erase(std::find(rest.begin(), rest.end(), elm1));
        coset.push_back(std::move(elm1));
      }
      ret.push_back(std::move(coset));
    }
    return ret;
  }

  std::vector<Element> conj_class(const Element& elm) const {
    std::unordered_set<Element> ret0{};
    for(const auto& g : elems)
      ret0.insert(g * elm * inverse(g));
    std::vector<Element> ret{};
    std::copy(ret0.begin(), ret0.end(), std::back_inserter(ret));
    return ret;
  }

private:
  auto index(const Element& elm) const {
    return std::distance(elems.begin(), std::find(elems.begin(), elems.end(), elm));
  }

};

template<class Group, class Matrix>
class Representation {
  const Group& group;
  std::vector<Matrix> elems;

public:
  Representation(const Group& group_, std::vector<Matrix> mxs)
    : group(group_), elems{Matrix{}}
  {
    assert(mxs.size() == group.gens.size());
    for(std::size_t i = 0; i < group.cayley.size(); i++) {
      const auto& c = group.cayley[i];
      for(std::size_t j = 0; j < group.gens.size(); j++) {
        if(c[j] >= elems.size()) {
          assert(c[j] == elems.size());
          elems.push_back(mxs[j]*elems[i]);
        }
      }
    }
  }

  auto begin() const {
    return elems.cbegin();
  }

  auto end() const {
    return elems.cend();
  }

  auto size() const {
    return elems.size();
  }

  const auto& represent(const typename Group::element_type& elm) const {
    return elems[group.index(elm)];
  }

  const auto& operator[](const typename Group::element_type& elm) const {
    return represent(elm);
  }

};


#endif
