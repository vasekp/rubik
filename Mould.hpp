#include <cstddef> // size_t
#include <cstdint> // uint16_t
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
      unsigned c1 = 0, c2 = 0;
      for(auto ix : f.vertices)
        if(Vertex vx = vertices[ix]; vx * p < 0)
          c1++;
        else if(vx * p > 0)
          c2++;
      // c1: count inside, c2: count outside (in +ve direction of the normal)
#ifdef DEBUG
      std::cout << "- " << c1 << " + " << c2 << '\n';
#endif
      if(c1 == 0 && discard) {
#ifdef DEBUG
        std::cout << "dropping { ";
        for(auto ix : f.vertices)
          std::cout << ix << ' ';
        std::cout << "}\n";
#endif
        continue;
      } else if(c1 == 0 || c2 == 0) {
        nfaces.push_back(std::move(f));
        continue;
      }
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
          if(!discard)
            fOut.vertices.push_back(newIx);
          nIn.vertices.push_back(newIx);
          if(!discard)
            nOut.vertices.push_back(newIx);
        }
        if(dot < 0)
          fIn.vertices.push_back(ix);
        else {
          if(dot == 0) {
            fIn.vertices.push_back(ix);
            nIn.vertices.push_back(ix);
          }
          if(!discard) {
            fOut.vertices.push_back(ix);
            nOut.vertices.push_back(ix);
          }
        }
        last = cur;
        ldot = dot;
      }
#ifdef DEBUG
      std::cout << "dropping { ";
      for(auto ix : f.vertices)
        std::cout << ix << ' ';
      std::cout << "}\n";
      std::cout << "adding { ";
      for(auto ix : fIn.vertices)
        std::cout << ix << ' ';
      std::cout << "}\n";
#endif
      nfaces.push_back(std::move(fIn));
      if(!discard) {
#ifdef DEBUG
        std::cout << "adding { ";
        for(auto ix : fOut.vertices)
          std::cout << ix << ' ';
        std::cout << "}\n";
#endif
        nfaces.push_back(std::move(fOut));
      }
    }
    nIn = convex_hull(nIn);
#ifdef DEBUG
    std::cout << "adding { ";
    for(auto ix : nIn.vertices)
      std::cout << ix << ' ';
    std::cout << "}\n";
#endif
    nfaces.push_back(std::move(nIn));
    if(!discard) {
      nOut = convex_hull(nOut);
#ifdef DEBUG
      std::cout << "adding { ";
      for(auto ix : nOut.vertices)
        std::cout << ix << ' ';
      std::cout << "}\n";
#endif
      nfaces.push_back(std::move(nOut));
    }
    swap(faces, nfaces);
  }

private:
  constexpr static float epsilon = 0.01;

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
};
