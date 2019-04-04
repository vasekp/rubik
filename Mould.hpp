#include <cstddef> // size_t
#include <cstdint> // uint16_t
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

  friend float operator*(const Vertex& v, const Plane& p) {
    return dot(p.normal, v) - p.offset;
  }
};

struct Face {
  std::vector<Index> vertices;
  glm::vec3 normal;
};

class Mould {
  std::vector<Vertex> vertices;
  std::vector<Face> faces;

public:

  Mould(float size = 2) {
    for(float x = -1; x <= 1; x += 2)
    for(float y = -1; y <= 1; y += 2)
    for(float z = -1; z <= 1; z += 2)
      vertices.push_back({size*x, size*y, size*z});
    faces.push_back({{0, 2, 3, 1}, {-1, 0, 0}});
    faces.push_back({{4, 5, 7, 6}, {1, 0, 0}});
    faces.push_back({{0, 1, 5, 4}, {0, -1, 0}});
    faces.push_back({{2, 6, 7, 3}, {0, 1, 0}});
    faces.push_back({{0, 4, 6, 2}, {0, 0, 1}});
    faces.push_back({{1, 3, 7, 5}, {0, 0, -1}});
  }

  void cut(Plane p, bool discard = false) {
    std::vector<Face> nfaces{};
    Face nIn{}, nOut{};
    nIn.normal = p.normal;
    nOut.normal = -p.normal;
    for(auto& f : faces) {
      unsigned cIn = 0, cOut = 0;
      for(auto ix : f.vertices)
        if(Vertex vx = vertices[ix]; vx * p < 0)
          cIn++;
        else if(vx * p > 0)
          cOut++;
#ifdef DEBUG
      std::cout << "- " << cIn << " + " << cOut << '\n';
#endif

      if(cOut == 0)
        keep_face(nfaces, std::move(f));
      if(cIn == 0) {
        if(!discard)
          keep_face(nfaces, std::move(f));
        else
          drop_face(f);
      }
      if(cIn == 0 || cOut == 0)
        continue;

      Face fIn{}, fOut{};
      const Vertex *last = &vertices[f.vertices.back()];
      float ldot = *last * p;
      for(auto ix : f.vertices) {
        const Vertex *cur = &vertices[ix];
        float dot = *cur * p;
        if(ldot * dot < 0) {
          auto intersect = (dot * *last - ldot * *cur)/(dot - ldot);
          auto [newIx, added] = find_or_append(std::move(intersect));
          fIn.vertices.push_back(newIx);
          nIn.vertices.push_back(newIx);
          if(!discard) {
            fOut.vertices.push_back(newIx);
            nOut.vertices.push_back(newIx);
          }
        }
        else if(dot == 0) {
          fIn.vertices.push_back(ix);
          nIn.vertices.push_back(ix);
          if(!discard) {
            fOut.vertices.push_back(ix);
            nOut.vertices.push_back(ix);
          }
        }
        if(dot < 0)
          fIn.vertices.push_back(ix);
        else if(dot > 0 && !discard)
          fOut.vertices.push_back(ix);
        last = cur;
        ldot = dot;
      }
      drop_face(std::move(f));
      add_face(nfaces, std::move(fIn));
      if(!discard)
        add_face(nfaces, std::move(fOut));
    }
    add_face(nfaces, convex_hull(nIn));
    if(!discard)
      add_face(nfaces, convex_hull(nOut));
    swap(faces, nfaces);
    cleanup();
  }

private:
  constexpr static float epsilon = 0.01;

  void add_face(std::vector<Face>& nfaces, Face&& face) {
#ifdef DEBUG
    std::cout << "adding { ";
    for(auto ix : face.vertices)
      std::cout << ix << ' ';
    std::cout << "}\n";
#endif
    nfaces.push_back(std::move(face));
  }

  // no-op (we just don't add it), but useful for debugging
  void drop_face(const Face& face) {
#ifdef DEBUG
    std::cout << "dropping { ";
    for(auto ix : face.vertices)
      std::cout << ix << ' ';
    std::cout << "}\n";
#endif
  }

  void keep_face(std::vector<Face>& nfaces, Face&& face) {
#ifdef DEBUG
    std::cout << "keeping { ";
    for(auto ix : face.vertices)
      std::cout << ix << ' ';
    std::cout << "}\n";
#endif
    nfaces.push_back(std::move(face));
  }

  std::pair<Index, bool> find_or_append(Vertex&& vx) {
    for(const auto& vy : vertices)
      if(distance(vx, vy) < epsilon)
        return {&vy - &vertices[0], false};
    // else
    vertices.push_back(std::move(vx));
    Index ix = static_cast<Index>(vertices.size() - 1);
#ifdef DEBUG
    std::cout << ix << ": {"
      << vertices[ix].x << ", "
      << vertices[ix].y << ", "
      << vertices[ix].z << "}\n";
#endif
    return {ix, true};
  }

  Face convex_hull(const Face& in) {
    Face out{};
    for(auto ix : in.vertices) {
      if(out.vertices.size() == 0) {
        out.vertices.push_back(ix);
        continue;
      } else if(out.vertices.size() == 1) {
        if(distance(vertices[out.vertices.front()], vertices[ix]) > epsilon)
          out.vertices.push_back(ix);
        continue;
      }

      auto vnew = vertices[ix];
      Index last = out.vertices.back();
      auto vlast = vertices[last];
      for(auto cur : out.vertices) {
        auto vcur = vertices[cur];
        if(dot(cross(vcur - vlast, vnew - vlast), in.normal) > 0) {
          out.vertices.insert(std::find(begin(out.vertices), end(out.vertices), cur), ix);
          break;
        }
        last = cur;
        vlast = vcur;
      }
    }
    out.normal = in.normal;
    return out;
  }

  void cleanup() {
    std::vector<bool> used(faces.size(), false);
    for(const auto& f : faces)
      for(auto ix : f.vertices)
        used[ix] = true;
    std::map<Index, Index> map{};
    size_t removed = 0;
    for(size_t i = 0, j = 0; j < vertices.size(); i++, j++) {
      while(!used[j]) {
#ifdef DEBUG
        std::cout << "Freeing vertex " << j << '\n';
#endif
        j++;
        removed++;
      }
      std::swap(vertices[i], vertices[j]);
      map[j] = i;
    }
    if(removed == 0)
      return;
    vertices.resize(vertices.size() - removed);
#ifdef DEBUG
    for(Index ix = 0; ix < vertices.size(); ix++) {
      std::cout << ix << ": {"
        << vertices[ix].x << ", "
        << vertices[ix].y << ", "
        << vertices[ix].z << "}\n";
    }
#endif
    for(auto& f : faces)
      for(auto& ix : f.vertices)
        ix = map[ix];
#ifdef DEBUG
    for(const auto& f : faces) {
      std::cout << "{ ";
      for(auto ix : f.vertices)
        std::cout << ix << ' ';
      std::cout << "}\n";
    }
#endif
  }
};
