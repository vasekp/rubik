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
using Vertex = glm::vec4;

struct Plane {
  glm::vec4 normal;
  float offset;

  friend float operator*(const Vertex& v, const Plane& p) {
    return dot(p.normal, v) - p.offset;
  }
};

struct Face {
  std::vector<Index> vertices;
  glm::vec4 normal;
};

class Mould {
  std::vector<Vertex> vertices;
  std::vector<Face> faces;

public:

  Mould(float size = 2) {
    for(float x = -1; x <= 1; x += 2)
    for(float y = -1; y <= 1; y += 2)
    for(float z = -1; z <= 1; z += 2)
      vertices.push_back({size*x, size*y, size*z, 1});
    faces.push_back({{0, 2, 3, 1}, {-1, 0, 0, 0}});
    faces.push_back({{4, 5, 7, 6}, {1, 0, 0, 0}});
    faces.push_back({{0, 1, 5, 4}, {0, -1, 0, 0}});
    faces.push_back({{2, 6, 7, 3}, {0, 1, 0, 0}});
    faces.push_back({{0, 4, 6, 2}, {0, 0, 1, 0}});
    faces.push_back({{1, 3, 7, 5}, {0, 0, -1, 0}});
  }

  void cut(Plane p, bool discard = false) {
    std::vector<Face> nfaces{};
    Face n1{}, n2{};
    n1.normal = p.normal;
    for(auto& f : faces) {
      unsigned c1 = 0, c2 = 0;
      for(auto ix : f.vertices)
        (vertices[ix] * p < 0 ? c1 : c2)++;
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
      Face f1{}, f2{};
      const Vertex *last = &vertices[f.vertices.back()];
      float ldot = *last * p;
      for(auto ix : f.vertices) {
        const Vertex *cur = &vertices[ix];
        float dot = *cur * p;
        if(ldot * dot < 0) {
          glm::vec4 intersect = (dot * *last - ldot * *cur)/(dot - ldot);
          auto [newIx, added] = find_or_append(std::move(intersect));
          f1.vertices.push_back(newIx);
          f2.vertices.push_back(newIx);
          if(added)
            n1.vertices.push_back(newIx);
        }
        (dot < 0 ? f1 : f2).vertices.push_back(ix);
        last = cur;
        ldot = dot;
      }
#ifdef DEBUG
      std::cout << "dropping { ";
      for(auto ix : f.vertices)
        std::cout << ix << ' ';
      std::cout << "}\n";
      std::cout << "adding { ";
      for(auto ix : f1.vertices)
        std::cout << ix << ' ';
      std::cout << "}\n";
#endif
      nfaces.push_back(std::move(f1));
      if(!discard) {
#ifdef DEBUG
        std::cout << "adding { ";
        for(auto ix : f2.vertices)
          std::cout << ix << ' ';
        std::cout << "}\n";
#endif
        nfaces.push_back(std::move(f2));
      }
    }
    n1 = convex_hull(n1);
#ifdef DEBUG
    std::cout << "adding { ";
    for(auto ix : n1.vertices)
      std::cout << ix << ' ';
    std::cout << "}\n";
#endif
    nfaces.push_back(std::move(n1));
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
    return {static_cast<Index>(vertices.size() - 1), true};
  }

  Face convex_hull(const Face& in) {
    Face out{};
    glm::vec3 normal{in.normal};
    for(auto ix : in.vertices) {
#ifdef DEBUG
      std::cout << ix << ": {"
        << vertices[ix].x << ", "
        << vertices[ix].y << ", "
        << vertices[ix].z << "}\n";
#endif
      if(out.vertices.size() == 0) {
        out.vertices.push_back(ix);
        continue;
      } else if(out.vertices.size() == 1) {
        if(distance(vertices[out.vertices.front()], vertices[ix]) > epsilon)
          out.vertices.push_back(ix);
        continue;
      }

      glm::vec3 vnew = glm::vec3{vertices[ix]};
      Index last = out.vertices.back();
      glm::vec3 vlast = glm::vec3{vertices[last]};
      for(auto cur : out.vertices) {
        glm::vec3 vcur = glm::vec3{vertices[cur]};
        if(dot(cross(vcur - vlast, vnew - vlast), normal) > 0) {
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
