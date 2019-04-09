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
};

struct Face {
  std::vector<Index> indices;
  glm::vec3 normal;

  Face(std::vector<Index>&& indices_, glm::vec3 normal_)
    : indices(std::move(indices_)), normal(normal_) { }

  Face(glm::vec3 normal_ = {})
    : Face({}, normal_) { }
};

class Volume {
public:
  std::vector<Vertex> vertices;
  std::vector<Face> faces;

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

  Volume cut(const Plane& p) {
    { // Simple cases
      unsigned cIn = 0, cOut = 0;
      for(const auto& vx : vertices)
        if(vx * p < 0)
          cIn++;
        else if(vx * p > 0)
          cOut++;

      if(cOut == 0) // keep all, return nothing
        return {};
      if(cIn == 0) { // discard all, return original      
        Volume ret{};
        std::swap(*this, ret);
        return ret;
      }
    }

    // volume actually cut by the plane

    Volume volIn{}, volOut{};
    std::vector<Face> facesIn{}, facesOut{};
    Face nIn{p.normal}, nOut{-p.normal};

    for(auto& f : faces) {
#ifdef DEBUG
      std::cout << "Face [ ";
      for(auto ix : f.indices)
        std::cout << ix << ' ';
      std::cout << "]: ";
#endif
      {
        unsigned cIn = 0, cOut = 0;
        for(auto ix : f.indices)
          if(Vertex vx = vertices[ix]; vx * p < 0)
            cIn++;
          else if(vx * p > 0)
            cOut++;

        if(cOut == 0) { // whole face in
#ifdef DEBUG
          std::cout << "keep\n";
#endif
          volIn.faces.push_back(std::move(f));
          continue;
        }
        if(cIn == 0) {  // whole face out
#ifdef DEBUG
          std::cout << "drop\n";
#endif
          volOut.faces.push_back(std::move(f));
          continue;
        }
      }

      // face actually cut by the plane
#ifdef DEBUG
      std::cout << "split\n";
#endif

      Face fIn{f.normal}, fOut{f.normal};
      Vertex last = vertices[f.indices.back()];
      float ldot = last * p;
      for(auto ix : f.indices) {
        Vertex cur = vertices[ix];
        float dot = cur * p;
        if(ldot * dot < 0) {  // the edge intersects the plane
          auto intersect = (dot * last - ldot * cur)/(dot - ldot);
          auto newIx = find_or_append(intersect);
          fIn.indices.push_back(newIx);
          nIn.indices.push_back(newIx);
          fOut.indices.push_back(newIx);
          nOut.indices.push_back(newIx);
        }
        if(dot < 0)
          fIn.indices.push_back(ix);
        else if(dot > 0)
          fOut.indices.push_back(ix);
        else { // the current vertex lies exactly on the plane: add to both
          fIn.indices.push_back(ix);
          nIn.indices.push_back(ix);
          fOut.indices.push_back(ix);
          nOut.indices.push_back(ix);
        }
        last = cur;
        ldot = dot;
      }
      volIn.faces.push_back(std::move(fIn));
      volOut.faces.push_back(std::move(fOut));
    }

    // add the new faces
    volIn.faces.push_back(sort_face(nIn));
    volOut.faces.push_back(sort_face(nOut));

    // take vertices from the original volume
    volIn.take_vertices_finalize(*this);
    volOut.take_vertices_finalize(*this);

    std::swap(*this, volIn);
    return volOut;
  }

  bool empty() const {
    return faces.empty();
  }

#ifdef DEBUG
  void dump() const {
    std::cout << "VOLUME:\n";
    for(Index ix = 0; ix < vertices.size(); ix++) {
      std::cout << ix << ": {"
        << vertices[ix].x << ", "
        << vertices[ix].y << ", "
        << vertices[ix].z << "}\n";
    }
    for(const auto& f : faces) {
      std::cout << "{ ";
      for(auto ix : f.indices)
        std::cout << ix << ' ';
      std::cout << "} n {"
        << f.normal.x << ", "
        << f.normal.y << ", "
        << f.normal.z << "}\n";
    }
  }
#endif

protected:
  Face sort_face(const Face& in) {
    Face out{in.normal};
    for(auto ix : in.indices) {
      if(out.indices.size() == 0) {
        out.indices.push_back(ix);
        continue;
      } else if(out.indices.size() == 1) {
        if(distance(vertices[out.indices[0]], vertices[ix]) > epsilon)
          out.indices.push_back(ix);
        continue;
      }

      auto vnew = vertices[ix];
      Index last = out.indices.back();
      auto vlast = vertices[last];
      for(auto cur : out.indices) {
        auto vcur = vertices[cur];
        if(dot(cross(vcur - vlast, vnew - vlast), in.normal) > 0) {
          out.indices.insert(std::find(begin(out.indices), end(out.indices), cur), ix);
          break;
        }
        last = cur;
        vlast = vcur;
      }
    }
    return out;
  }

private:
  constexpr static float epsilon = 0.01;

  Index find_or_append(const Vertex& vx) {
    for(const auto& vy : vertices)
      if(distance(vx, vy) < epsilon)
        return static_cast<Index>(&vy - &vertices[0]);
    // else
    vertices.push_back(vx);
    Index ix = static_cast<Index>(vertices.size() - 1);
#ifdef DEBUG
    std::cout << "Adding " << ix << ": {"
      << vertices[ix].x << ", "
      << vertices[ix].y << ", "
      << vertices[ix].z << "}\n";
#endif
    return ix;
  }

  void take_vertices_finalize(const Volume& orig) {
    Index newIx = 0;
    std::map<Index, Index> map{};
    for(auto& f : faces)
      for(auto& ix : f.indices)
        if(auto it = map.find(ix); it != map.end())
          ix = it->second;
        else {
          vertices.push_back(orig.vertices[ix]);
          ix = map[ix] = newIx++;
        }
#ifdef DEBUG
    std::cout << "Taking";
    for(auto [key, val] : map)
      std::cout << ' ' << key;
    std::cout << '\n';
#endif
  }
};

class ExtVolume : public Volume {
  public:
  std::vector<Face> ext_faces;

  ExtVolume(const Volume& orig, float dist) {
#ifdef DEBUG
    std::cout << "\nEXT VOLUME\n";
#endif
    std::map<Index, Face> ext_vertices{};

    // copy faces, blow up vertices
    // each face gets a unique set of new vertices
    // new vertex indices predictable
    for(const auto& f : orig.faces) {
      size_t base = vertices.size();
      size_t newIx = base;
      std::vector<Index> newIxs{};
      for(auto ix : f.indices) {
        vertices.push_back(orig.vertices[ix]); // new element @ newIx
        ext_vertices[ix].indices.push_back(newIx);
        ext_vertices[ix].normal += f.normal;
        newIxs.push_back(newIx++);
      }
      faces.push_back({std::move(newIxs), std::move(f.normal)});
    }

    // find coincident edges, make edge faces
    for(size_t fc = orig.faces.size(), fi1 = 0; fi1 < fc; fi1++) {
      const auto& f1 = orig.faces[fi1];
      size_t vc1 = f1.indices.size();
      for(size_t vi1 = 0; vi1 < vc1; vi1++) {
        size_t vj1 = (vi1 + 1) % vc1;
        Index vix1 = f1.indices[vi1],
              vjx1 = f1.indices[vj1];
        bool found = false;
        // [find vi1->vj1 edge in another face]
        for(size_t fi2 = fi1 + 1; !found && fi2 < fc; fi2++) {
          const auto& f2 = orig.faces[fi2];
          size_t vc2 = f2.indices.size();
          for(size_t vi2 = 0; vi2 < vc2; vi2++) {
            size_t vj2 = (vi2 + 1) % vc2;
            Index vix2 = f2.indices[vi2],
                  vjx2 = f2.indices[vj2];
            // must be oriented opposite
            if(vix1 == vjx2 && vix2 == vjx1) {
              // edge found!
              found = true;
#ifdef DEBUG
              std::cout << 'f' << fi1 << " i" << vi1 << " j" << vj1 <<
                " = f" << fi2 << " i" << vi2 << " j" << vj2 << '\n';
#endif
              // vi1, vj1 indices within f1 = orig.faces[fi1]
              // vi2, vj2 indices within f2 = orig.faces[fi2]
              // we need to reindex to new vertices, i.e., take vector indices from new faces
              std::vector<Index> edge_ixs = {
                faces[fi1].indices[vj1],
                faces[fi1].indices[vi1],
                faces[fi2].indices[vj2],
                faces[fi2].indices[vi2]
              };
              ext_faces.push_back({std::move(edge_ixs), f1.normal + f2.normal});
              break;
            } // found
          } // vi2
        } // fi2
      } // vi1
    } // fi1

    // shrink all new faces (affecting vertices referenced from all structures)
    for(const auto& f : faces) {
      auto& normal = f.normal;
      size_t size = f.indices.size();
      auto* vprev = &vertices[f.indices.back()];
      auto* vthis = &vertices[f.indices.front()];
      std::vector<Vertex> shrunk(size);
      for(size_t i = 0; i < size; i++) {
        auto* vnext = &vertices[f.indices[(i + 1) % size]];
        auto dprev = normalize(*vthis - *vprev);
        auto dnext = normalize(*vnext - *vthis);
        auto cross = glm::cross(dprev, normal);
        auto triple = dot(dnext, cross);
#ifdef DEBUG
        std::cout << "Vertex " << f.indices[i] << ": "
          << "|dprev| = " << length(dprev)
          << ", |dnext| = " << length(dnext)
          << ", |cross| = " << length(cross)
          << ", triple = " << triple << '\n';
#endif
        shrunk[i] = *vthis + dist * cross - triple * dist / (1 + dot(dprev, dnext)) * dprev;
        vprev = vthis;
        vthis = vnext;
      }
      // safe to overwrite now
      for(size_t i = 0; i < size; i++) {
#ifdef DEBUG
        std::cout << "Vertex " << f.indices[i] << " shrunk by "
          << distance(vertices[f.indices[i]], shrunk[i]) << '\n';
#endif
        vertices[f.indices[i]] = shrunk[i];
      }
    }

    // fix all new faces
    for(auto& [ix, f] : ext_vertices)
      ext_faces.push_back(sort_face(f));

    for(auto& f : ext_faces)
      f.normal = normalize(f.normal);

#ifdef DEBUG
    std::cout << "Totals: "
      << faces.size() << " faces, "
      << vertices.size() << " vertices, "
      << ext_faces.size() << " ext faces\n";
#endif
  }
};

class Mould {
  std::vector<Volume> volumes;

public:

  Mould(Volume v) {
    volumes.push_back(std::move(v));
  }

  void cut(const Plane& p) {
#ifdef DEBUG
    std::cout << "\nCUTTING\n";
#endif
    std::vector<Volume> nvolumes{};
    for(auto& volume : volumes) {
#ifdef DEBUG
      std::cout << "[ vol ]\n";
#endif
      Volume outer = volume.cut(p);
      if(volume.empty())
        std::swap(volume, outer);
      if(!outer.empty())
        nvolumes.push_back(std::move(outer));
    }
    using MoveIter = std::move_iterator<decltype(begin(nvolumes))>;
    std::copy(MoveIter{begin(nvolumes)}, MoveIter{end(nvolumes)}, std::back_inserter(volumes));
#ifdef DEBUG
    for(const auto& v : volumes)
      v.dump();
#endif
  }

  const std::vector<Volume>& get_volumes() const {
    return volumes;
  }
};
