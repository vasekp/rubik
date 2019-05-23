#ifndef MOULD_HPP
#define MOULD_HPP

#include <cmath>
#include <cstddef> // size_t
#include <cstdint> // uint16_t
#include <iterator>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>

#ifdef DEBUG
#include <iostream>
#endif

using std::size_t;
using Index = uint16_t;
using Vertex = glm::vec3;

struct Plane {
  glm::vec3 normal;
  float offset;

  Plane(const glm::vec3& normal_, float offset_)
    : normal(normalize(normal_)), offset(offset_ / length(normal_)) { }

  friend float operator*(const Vertex& v, const Plane& p) {
    return dot(p.normal, v) - p.offset;
  }

  friend float operator*(const Plane& p, const Vertex& v) {
    return dot(p.normal, v) - p.offset;
  }
};

struct Cut {
  Plane plane;
  Index tag;
};

struct Face {
  std::vector<Index> indices;
  glm::vec3 normal;
  Index tag;

  Face(std::vector<Index>&& indices_, glm::vec3 normal_, Index tag_ = 0)
    : indices(std::move(indices_)), normal(normal_), tag(tag_) { }

  Face(glm::vec3 normal_, Index tag_)
    : Face({}, normal_, tag_) { }

  Face() : Face({}, 0) { }

  int index(Index ix) const {
    return std::distance(indices.begin(), std::find(indices.begin(), indices.end(), ix));
  }

  Index operator[](int i) const {
    assert(i >= -1);
    return i == -1 ? indices.back() : indices[i % indices.size()];
  }

  Index next(Index ix) const {
    return (*this)[index(ix) + 1];
  }

  Index prev(Index ix) const {
    return (*this)[index(ix) - 1];
  }

#ifdef DEBUG
  friend std::ostream& operator<<(std::ostream& os, const Face& f) {
    os << "[ ";
    for(auto ix : f.indices)
      os << ix << ' ';
    os << ']';
    return os;
  }
#endif
};

class Volume {
protected:
  std::vector<Vertex> vertices;
  std::vector<Face> faces;

public:
  Volume() = default;
  Volume(float size);

  const std::vector<Vertex>& get_vertices() const { return vertices; }
  const std::vector<Face>& get_faces() const { return faces; }
  bool empty() const { return faces.empty(); }

  Vertex center() const;

  Volume cut(const Plane& p, Index tag = 0);
  void erode(float dist);
  void dilate(float dist);

  constexpr static Index dilate_face_tag = -1;

#ifdef DEBUG
  void dump() const;
#endif

private:
  constexpr static float epsilon = 0.001;

  void add_intersections(const Plane& p);
  std::vector<Index> find_section(const Plane& p);
  std::vector<Index> traverse_start(const Face& f, const Plane& p);
  std::vector<Index> traverse_nbours(std::vector<Index> section, const Plane& p, Index ixPivot, Index ixNeg, Index ixPos);
  std::vector<Index> traverse_face(std::vector<Index> section, const Plane& p, const Face& face, Index ixNeg, Index ixPos);
  Face& find_face(Index i1, Index i2);
  void take_vertices_finalize(const Volume& orig);
};

class ExtVolume : public Volume {
  std::vector<Face> ext_faces;

public:
  ExtVolume(const Volume& orig) {
    const auto& orig_vertices = orig.get_vertices();
    const auto& orig_faces = orig.get_faces();
    for(const auto& f : orig_faces) {
      size_t base = vertices.size();
      size_t newIx = base;
      std::vector<Index> newIxs{};
      for(auto ix : f.indices) {
        vertices.push_back(orig_vertices[ix]); // new element @ newIx
        newIxs.push_back(newIx++);
      }
      faces.emplace_back(std::move(newIxs), f.normal, f.tag);
    }
  }

  const std::vector<Face>& get_ext_faces() const { return ext_faces; }
};

class Mould {
  std::vector<Volume> volumes;

public:

  Mould(Volume v) {
    volumes.push_back(std::move(v));
  }

  void cut(const Plane& p, Index tag = 0) {
#ifdef DEBUG
    std::clog << "\nMould::cut\n";
#endif
    std::vector<Volume> nvolumes{};
    for(auto& volume : volumes) {
      Volume outer = volume.cut(p, tag);
      if(volume.empty())
        std::swap(volume, outer);
      if(!outer.empty())
        nvolumes.push_back(std::move(outer));
    }
    std::copy(std::make_move_iterator(begin(nvolumes)), std::make_move_iterator(end(nvolumes)),
        std::back_inserter(volumes));
#ifdef DEBUG
    std::clog << '\n' << volumes.size() << '\n';
#endif
  }

  const std::vector<Volume>& get_volumes() const {
    return volumes;
  }
};

#endif
