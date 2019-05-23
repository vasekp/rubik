#ifndef MOULD_HPP
#define MOULD_HPP

#include <cmath>
#include <cstddef> // size_t
#include <cstdint> // uint16_t
#include <iterator>
#include <vector>
#include <map>
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

  unsigned index(Index ix) const {
    return static_cast<unsigned>(std::distance(indices.begin(), std::find(indices.begin(), indices.end(), ix)));
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

  Volume(float size) {
    for(float x = -1; x <= 1; x += 2)
    for(float y = -1; y <= 1; y += 2)
    for(float z = -1; z <= 1; z += 2)
      vertices.push_back({size*x, size*y, size*z});
    faces.push_back({{0, 2, 3, 1}, {-1, 0, 0}});
    faces.push_back({{4, 5, 7, 6}, {1, 0, 0}});
    faces.push_back({{0, 1, 5, 4}, {0, -1, 0}});
    faces.push_back({{2, 6, 7, 3}, {0, 1, 0}});
    faces.push_back({{0, 4, 6, 2}, {0, 0, -1}});
    faces.push_back({{1, 3, 7, 5}, {0, 0, 1}});
  }

  const std::vector<Vertex>& get_vertices() const { return vertices; }

  const std::vector<Face>& get_faces() const { return faces; }

  Volume cut(const Plane& p, Index tag = 0) {
#ifdef DEBUG
    std::clog << "\nVolume::cut\n";
#endif

    // Simple cases
    if(std::all_of(vertices.begin(), vertices.end(), [&p](const Vertex& vx) -> bool { return vx * p < epsilon; })) {
#ifdef DEBUG
      std::clog << "[keep all]\n";
#endif
      return {};
    } else if(std::all_of(vertices.begin(), vertices.end(), [&p](const Vertex& vx) -> bool { return vx * p > -epsilon; })) {
#ifdef DEBUG
      std::clog << "[drop all]\n";
#endif
      Volume ret{};
      std::swap(*this, ret);
      return ret;
    }

    add_intersections(p);

    Volume volIn{}, volOut{};
    std::vector<Index> section = find_section(p);
    assert(!section.empty());
#ifdef DEBUG
    std::clog << "Section: [ ";
    for(auto ix : section)
      std::clog << ix << ' ';
    std::clog << "]\n";
#endif
    std::vector<Index> secReverse = section;
    std::reverse(secReverse.begin(), secReverse.end());
    volIn.faces.emplace_back(std::move(section), p.normal, tag);
    volOut.faces.emplace_back(std::move(secReverse), -p.normal, tag);

    for(const auto& face : faces) {
      Face fIn{face.normal, face.tag}, fOut{face.normal, face.tag};
      for(auto ix : face.indices) {
        auto dot = vertices[ix] * p;
        if(dot < epsilon)
          fIn.indices.push_back(ix);
        if(dot > -epsilon)
          fOut.indices.push_back(ix);
      }
      if(fIn.indices.size() > 2)
        volIn.faces.push_back(std::move(fIn));
      if(fOut.indices.size() > 2)
        volOut.faces.push_back(std::move(fOut));
    }

    // take vertices from the original volume
    volIn.take_vertices_finalize(*this);
    volOut.take_vertices_finalize(*this);

#ifdef DEBUG
    volIn.dump();
    volOut.dump();
#endif

    std::swap(*this, volIn);
    return volOut;
  }

  bool empty() const {
    return faces.empty();
  }

  Vertex center() const {
    Vertex ret{};
    for(const auto& vx : vertices)
      ret += vx;
    return ret / float(vertices.size());
  }

#ifdef DEBUG
  void dump() const {
    std::clog << "VOLUME:\n";
    for(Index ix = 0; ix < vertices.size(); ix++) {
      std::clog << ix << ": {"
        << vertices[ix].x << ", "
        << vertices[ix].y << ", "
        << vertices[ix].z << "}\n";
    }
    for(const auto& f : faces) {
      std::clog << "{ ";
      for(auto ix : f.indices)
        std::clog << ix << ' ';
      std::clog << "} n {"
        << f.normal.x << ", "
        << f.normal.y << ", "
        << f.normal.z << "}\n";
    }
  }
#endif

  void erode(float dist) {
    std::vector<Cut> cuts{};
    for(const auto& face : faces)
      cuts.push_back({{face.normal, glm::dot(face.normal, vertices[face.indices.front()]) - dist}, face.tag});
    for(const auto& cut : cuts)
      this->cut(cut.plane, cut.tag);
  }

  void dilate(float dist) {
    decltype(vertices) nvertices;
    decltype(faces) nfaces;
    Index ix_new{0};

    // Give each face its own set of vertices
    for(const auto& face : faces) {
      Face nface{face.normal, face.tag};
      for(auto ix : face.indices) {
        nvertices.push_back(vertices[ix]);
        nface.indices.push_back(ix_new++);
      }
      nfaces.push_back(std::move(nface));
    }

    // Make new (zero area) faces for edges
    for(const auto& face : faces) {
      size_t i = &face - &faces[0];
      size_t sz = face.indices.size();
      for(size_t j = 0; j < sz; j++)
        if(face.indices[j] < face.indices[(j + 1) % sz]) { // This condition guarantees that each edge is only counted once
          Index orig1 = face.indices[j], orig2 = face.indices[(j + 1) % sz];
          const Face& face2 = find_face(orig2, orig1);
          size_t i2 = &face2 - &faces[0];
          size_t sz2 = face2.indices.size();
          size_t j2 = face2.index(orig2);
          nfaces.push_back({{
              nfaces[i].indices[(j + 1) % sz], nfaces[i].indices[j],
              nfaces[i2].indices[(j2 + 1) % sz2], nfaces[i2].indices[j2]},
            glm::normalize(face.normal + face2.normal), Index(-1)});
        }
    }

    // Make new zero area faces for vertices
    std::vector<bool> seen(vertices.size());
    for(const auto& face : faces) {
      Index i = &face - &faces[0];
      size_t sz = face.indices.size();
      for(size_t j = 0; j < sz; j++) {
        Index pivot = face.indices[j];
        if(seen[pivot])
          continue;
        Index ix = face.indices[(j + 1) % face.indices.size()];
        Index nix = nfaces[i].indices[j % face.indices.size()];
        glm::vec3 normal{};
        std::vector<Index> nface{nix};
        for(;;) {
          const Face& f2 = find_face(ix, pivot);
          size_t i2 = &f2 - &faces[0];
          auto j2 = f2.index(pivot);
          Index ix2 = f2.indices[(j2 + 1) % f2.indices.size()];
          Index nix2 = nfaces[i2].indices[j2 % f2.indices.size()];
          if(nix2 == nface.front())
            break;
          nface.push_back(nix2);
          ix = ix2;
        }
        std::reverse(nface.begin(), nface.end());
        nfaces.emplace_back(std::move(nface), normal, Index(-1));
        seen[pivot] = true;
      }
    }

    // Displace vertices
    for(const auto& nface : nfaces) {
      if(nface.tag == Index(-1)) // Ignore extra faces
        continue;
      glm::vec3 displ = dist * nface.normal;
      for(auto ix : nface.indices)
        nvertices[ix] += displ;
    }

    std::swap(vertices, nvertices);
    std::swap(faces, nfaces);
  }

private:
  constexpr static float epsilon = 0.001;

  void add_intersections(const Plane& p) {
    for(auto& face : faces) {
      Index last_ix = face.indices.back();
      auto last_dot = vertices[last_ix] * p;
      for(unsigned i = 0; i < face.indices.size(); i++) {
        Index cur_ix = face.indices[i];
        auto cur_dot = vertices[cur_ix] * p;
        if((cur_dot > epsilon && last_dot < -epsilon) || (cur_dot < -epsilon && last_dot > epsilon)) {
          Vertex new_vx = (cur_dot * vertices[last_ix] - last_dot * vertices[cur_ix]) / (cur_dot - last_dot);
          Index new_ix = static_cast<Index>(vertices.size());
          vertices.push_back(new_vx);
          Face& f2 = find_face(cur_ix, last_ix);
#ifdef DEBUG
          std::clog << "New vertex between " << last_ix << " and " << cur_ix << ": "
            << new_vx.x << ", " << new_vx.y << ", " << new_vx.z << " [" << new_ix << "]\n";
          std::clog << "Adding to " << face << " and " << f2 << '\n';
#endif
          face.indices.insert(face.indices.begin() + i, new_ix);
          f2.indices.insert(std::find(f2.indices.begin(), f2.indices.end(), last_ix), new_ix);
#ifdef DEBUG
          std::clog << "-> " << face << " and " << f2 << '\n';
#endif
        }
        std::tie(last_ix, last_dot) = std::tie(cur_ix, cur_dot);
      }
    }
  }

  std::vector<Index> find_section(const Plane& p) {
    for(const auto& face : faces) {
      bool hasPos = false, hasNeg = false;
      for(auto ix : face.indices) {
        auto dot = vertices[ix] * p;
        if(dot > epsilon)
          hasPos = true;
        if(dot < -epsilon)
          hasNeg = true;
      }
      if(hasPos && hasNeg)
        return traverse_start(face, p);
    }
    throw std::runtime_error("find_section() failed");
  }

  std::vector<Index> traverse_start(const Face& f, const Plane& p) {
    unsigned i, first_nonneg;
    for(i = 0; i < f.indices.size(); i++)
      if(vertices[f.indices[i]] * p < -epsilon)
        break;
    for(; i < f.indices.size(); i++)
      if(vertices[f.indices[i]] * p > -epsilon) {
        first_nonneg = i;
        break;
      }
    if(i == f.indices.size())
      first_nonneg = 0;
    unsigned before = (first_nonneg + f.indices.size() - 1) % f.indices.size();
    unsigned after = (first_nonneg + 1) % f.indices.size();
    Index ixPivot = f.indices[first_nonneg];
    Index ixNeg = f.indices[before];
    Index ixPos = f.indices[after];
    assert(vertices[ixNeg] * p < -epsilon && vertices[ixPos] * p > -epsilon);
    return traverse_nbours({ixPivot}, p, ixPivot, ixNeg, ixPos);
  }

  std::vector<Index> traverse_nbours(std::vector<Index> section, const Plane& p, Index ixPivot, Index ixNeg, Index ixPos) {
#ifdef DEBUG
    std::clog << "Traverse neighbours: "
      << "ixPivot = " << ixPivot
      << ", ixNeg = " << ixNeg
      << ", ixPos = " << ixPos << '\n';
#endif
    assert(abs(vertices[ixPivot] * p) < epsilon && vertices[ixNeg] * p < epsilon && vertices[ixPos] * p > -epsilon);
    Index ix = ixNeg;
    for(;;) {
#ifdef DEBUG
      std::clog << " ... " << ix << " (" << vertices[ix] * p << ")\n";
#endif
      if(abs(vertices[ix] * p) < epsilon) {
        if(section.front() == ix) // loop closed, done
          return section;
        else {
          section.push_back(ix);
          const Face& prev_face = find_face(ix, ixPivot);
          const Face& next_face = find_face(ixPivot, ix);
          Index new_pos = next_face.indices[(next_face.index(ix) + 1) % next_face.indices.size()];
          Index new_neg = prev_face.indices[(prev_face.index(ix) + prev_face.indices.size() - 1) % prev_face.indices.size()];
          return traverse_nbours(section, p, ix, new_neg, new_pos);
        }
      } else if(vertices[ix] * p > epsilon) {
        assert(ix != ixNeg);
        const Face& prev_face = find_face(ix, ixPivot);
        Index neg = prev_face.indices[(prev_face.index(ixPivot) + 1) % prev_face.indices.size()];
        return traverse_face(section, p, prev_face, neg, ix);
      }
      if(ix == ixPos)
        throw std::runtime_error("traverse_nbours() failed!");
      const Face& next_face = find_face(ixPivot, ix);
      ix = next_face.indices[(next_face.index(ixPivot) + next_face.indices.size() - 1) % next_face.indices.size()];
    }
  }

  std::vector<Index> traverse_face(std::vector<Index> section, const Plane& p, const Face& face, Index ixNeg, Index ixPos) {
#ifdef DEBUG
    std::clog << "Traverse face: face = " << face
      << ", ixNeg = " << ixNeg
      << ", ixPos = " << ixPos << '\n';
#endif
    assert(vertices[ixNeg] * p < -epsilon && vertices[ixPos] * p > epsilon);
    for(unsigned i = face.index(ixNeg); face.indices[i] != ixPos; i = (i + 1) % face.indices.size()) {
      Index ix = face.indices[i];
#ifdef DEBUG
      std::clog << " ... " << ix << " (" << vertices[ix] * p << ")\n";
#endif
      if(abs(vertices[ix] * p) < epsilon) {
        if(section.front() == ix) // loop closed
          return section;
        else {
          section.push_back(ix);
          Index ixNeg = face.indices[(i + face.indices.size() - 1) % face.indices.size()];
          Index ixPos = face.indices[(i + 1) % face.indices.size()];
          return traverse_nbours(section, p, ix, ixNeg, ixPos);
        }
      }
    }
    throw std::runtime_error("traverse_face() failed!");
  }

  Face& find_face(Index i1, Index i2) {
    for(auto& face : faces) {
      auto sz = face.indices.size();
      for(unsigned i = 0; i < sz; i++)
        if(face.indices[i] == i1 && face.indices[(i + 1) % sz] == i2)
          return face;
    }
    throw std::runtime_error("face not found");
  }

  void take_vertices_finalize(const Volume& orig) {
    Index newIx = 0;
    std::map<Index, Index> map{};
#ifdef DEBUG
    std::clog << "Remap: ";
#endif
    for(auto& f : faces)
      for(auto& ix : f.indices)
        if(auto it = map.find(ix); it != map.end())
          ix = it->second;
        else {
          vertices.push_back(orig.vertices[ix]);
#ifdef DEBUG
          std::clog << ix << "→" << newIx << ' ';
#endif
          ix = map[ix] = newIx++;
        }
#ifdef DEBUG
    std::clog << '\n';
#endif
  }
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
