#ifndef SOLID_HPP
#define SOLID_HPP

#include <cmath>
#include <utility>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Group.hpp"
#include "Permutation.hpp"

class Solid {
  
  Group<Permutation> group;
  Representation<decltype(group), glm::mat4> rep;
  Permutation p_face;
  Permutation p_vertex;
  Permutation p_edge;
  glm::vec3 v_face;
  glm::vec3 v_vertex;
  glm::vec3 v_edge;

  Solid(Permutation a, Permutation b, glm::vec3 va, glm::vec3 vb, glm::vec3 vc)
    : group({a, b}),
      rep({group, {
            glm::rotate(glm::mat4{}, -float(2*M_PI/a.order()), va),
            glm::rotate(glm::mat4{}, -float(2*M_PI/b.order()), vb)
          }}),
      p_face(a), p_vertex(b), p_edge(b*a),
      v_face(va), v_vertex(vb), v_edge(vc)
    { }

public:

  static Solid platonic(unsigned p, unsigned q) {
    if((p != 3 && q != 3) || p+q < 6 || p+q > 8)
      throw std::logic_error("p, q do not define a Platonic solid");
    Permutation a, b;
    glm::vec3 va, vb, vc;
    switch(p) {
      case 3:
        switch(q) {
          case 3: // tetrahedron
            a = {{1, 2, 0, 3}};
            b = {{0, 2, 3, 1}};
            break;
          case 4: // octahedron
            a = {{0, 2, 3, 1}};
            b = {{3, 0, 1, 2}};
            break;
          case 5: // icosahedron
            a = {{0, 2, 4, 3, 1}};
            b = {{4, 0, 1, 2, 3}};
            break;
        }
        break;
      case 4: // cube
        a = {{1, 2, 3, 0}};
        b = {{0, 3, 1, 2}};
        break;
      case 5: // dodecahedron
        a = {{1, 2, 3, 4, 0}};
        b = {{0, 4, 1, 3, 2}};
        break;
    }
    assert(a.order() == p);
    assert(b.order() == q);
    assert((b*a).order() == 2);
    float alpha = M_PI/p, beta = M_PI/q;
    float chi = acos(1 / tan(alpha) / tan(beta)); // angle between face center and vertex
    va = {0, -cos(chi), 0}; // first face normal downwards
    vc = {sin(chi)*cos(alpha), -cos(chi), 0}; // first edge XY plane, positive X
    vb = {sin(chi)*cos(alpha), -cos(chi), -sin(chi)*sin(alpha)}; // first vertex towards us
    // Note:
    //   vc == {cos(thetaHalf), -sin(thetaHalf), 0}
    //   where thetaHalf = asin(cos(beta) / sin(alpha)); // dihedral angle
    return {a, b, va, vb, vc};
  }

  using direction_pair = std::pair<Permutation::size_type, glm::vec3>;

  std::vector<direction_pair> face_dirs() const {
    return dirs(p_face, v_face);
  }

  std::vector<direction_pair> vertex_dirs() const {
    return dirs(p_vertex, v_vertex);
  }

  std::vector<direction_pair> edge_dirs() const {
    return dirs(p_edge, v_edge);
  }

  float r_face() const {
    return glm::length(v_face);
  }

  float r_vertex() const {
    return glm::length(v_vertex);
  }

  float r_edge() const {
    return glm::length(v_edge);
  }

private:
  std::vector<direction_pair>
    dirs(const Permutation& generator, const glm::vec3& ref_vector) const
  {
    Group<Permutation> subgroup{{generator}};
    std::vector<direction_pair> ret{};
    for(const auto& coset : group.cosets_r(subgroup)) {
      glm::mat3 matrix = {rep.represent(coset.front())};
      ret.push_back({Permutation::to_numbered(coset.front()), matrix * ref_vector});
    }
    return ret;
  }

};

#endif
