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

struct Plane {
  glm::vec4 normal;
  float offset;
};

struct Vertex {
  glm::vec4 coord;
  glm::vec4 normal;

  friend float operator*(const Vertex& v, const Plane& p) {
    return dot(p.normal, v.coord) - p.offset;
  }
};

class Mould {
  using Face = std::vector<Index>;
  std::vector<Vertex> vertices;
  std::vector<Face> faces;

public:

  Mould(float size = 2) {
    for(float x = -1; x <= 1; x += 2)
    for(float y = -1; y <= 1; y += 2)
    for(float z = -1; z <= 1; z += 2)
      vertices.push_back({{size*x, size*y, size*z, 1}, {}});
    faces.push_back({0, 2, 3, 1});
    faces.push_back({4, 5, 7, 6});
    faces.push_back({0, 1, 5, 4});
    faces.push_back({2, 6, 7, 3});
    faces.push_back({0, 4, 6, 2});
    faces.push_back({1, 3, 7, 5});
  }

  void cut(Plane p, bool discard = false) {
    std::vector<Face> nfaces{};
    Face n1{}, n2{};
    for(auto& f : faces) {
      unsigned c1 = 0, c2 = 0;
      for(auto ix : f)
        (vertices[ix] * p < 0 ? c1 : c2)++;
      // c1: count inside, c2: count outside (in +ve direction of the normal)
#ifdef DEBUG
      std::cout << "- " << c1 << " + " << c2 << '\n';
#endif
      if(c1 == 0 && discard) {
#ifdef DEBUG
        std::cout << "dropping { ";
        for(auto ix : f)
          std::cout << ix << ' ';
        std::cout << "}\n";
#endif
        continue;
      } else if(c1 == 0 || c2 == 0) {
        nfaces.push_back(std::move(f));
        continue;
      }
      Face f1{}, f2{};
      const Vertex *last = &vertices[f.back()];
      float ldot = *last * p;
      for(auto ix : f) {
        const Vertex *cur = &vertices[ix];
        float dot = *cur * p;
        if(ldot * dot < 0) {
          glm::vec4 intersect = (dot * last->coord - ldot * cur->coord)/(dot - ldot);
          f1.push_back(new_vertex({intersect,{0, 0, 0, 0}}));
          f2.push_back(new_vertex({intersect,{0, 0, 0, 0}}));
          n1.push_back(new_vertex({intersect, p.normal}));
        }
        (dot < 0 ? f1 : f2).push_back(ix);
        last = cur;
        ldot = dot;
      }
#ifdef DEBUG
      std::cout << "dropping { ";
      for(auto ix : f)
        std::cout << ix << ' ';
      std::cout << "}\n";
      std::cout << "adding { ";
      for(auto ix : f1)
        std::cout << ix << ' ';
      std::cout << "}\n";
#endif
      nfaces.push_back(std::move(f1));
      if(!discard) {
#ifdef DEBUG
        std::cout << "adding { ";
        for(auto ix : f2)
          std::cout << ix << ' ';
        std::cout << "}\n";
#endif
        nfaces.push_back(std::move(f2));
      }
    }
    // fix the new face
    {
      constexpr float epsilon = 0.01;

      Face nn1{};
      // assert n1.size() >= 3
      for(auto ix : n1) {
#ifdef DEBUG
        std::cout << ix << ": {"
          << vertices[ix].coord.x << ", "
          << vertices[ix].coord.y << ", "
          << vertices[ix].coord.z << "}, {"
          << vertices[ix].normal.x << ", "
          << vertices[ix].normal.y << ", "
          << vertices[ix].normal.z << "}\n";
#endif
        if(nn1.size() == 0) {
          nn1.push_back(ix);
          continue;
        } else if(nn1.size() == 1) {
          if(distance(vertices[nn1[0]].coord, vertices[ix].coord) > epsilon)
            nn1.push_back(ix);
          continue;
        }

        bool found;
        glm::vec3 vnew = glm::vec3{vertices[ix].coord};
        Index last = nn1.back();
        glm::vec3 vlast = glm::vec3{vertices[last].coord};
        for(auto cur : nn1) {
          glm::vec3 vcur = glm::vec3{vertices[cur].coord};
          if(dot(cross(vcur - vlast, vnew - vlast), glm::vec3{vertices[ix].normal}) > 0) {
            nn1.insert(std::find(begin(nn1), end(nn1), cur), ix);
            found = true;
            break;
          }
          last = cur;
          vlast = vcur;
        }
        // if(!found) remove_vx();
      }
      swap(n1, nn1);
    }
#ifdef DEBUG
    std::cout << "adding { ";
    for(auto ix : n1)
      std::cout << ix << ' ';
    std::cout << "}\n";
#endif
    nfaces.push_back(std::move(n1));
    swap(faces, nfaces);
  }

  Index new_vertex(Vertex&& vx) {
    vertices.push_back(std::move(vx));
    return static_cast<Index>(vertices.size() - 1);
  }
};
