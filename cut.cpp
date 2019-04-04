#define DEBUG
#include "Mould.hpp"
#include "GL/gl.h"

void append_face_list(std::vector<Index>& indices, const std::vector<Face>& faces) {
  for(const auto& face : faces) {
    Index base = face.vertices[0];
    Index prev = face.vertices[1];
    for(auto ix : face.vertices) {
      if(ix == base || ix == prev)
        continue;
      indices.push_back(base);
      indices.push_back(prev);
      indices.push_back(ix);
      prev = ix;
    }
  }
}

int main() {
  Mould m{};
  m.cut({glm::normalize(glm::vec3{1, 1, 1}), 0}, true);
  m.cut({glm::normalize(glm::vec3{-1, 1, 1}), 0}, true);
  m.cut({glm::normalize(glm::vec3{-1, 1, -1}), 0}, true);
  m.cut({glm::normalize(glm::vec3{1, 1, -1}), 0}, true);

  auto volumes = m.get_volumes_clipped();
  std::vector<glm::vec3> coords{};
  std::vector<glm::vec3> normals{};
  std::vector<Index> indices{};
  {
    size_t vx_total = 0, ix_total = 0, face_count = 0;
    for(const auto& volume : volumes) {
      vx_total += volume.vertex_coords.size();
      face_count += volume.ext_faces.size() + volume.ext_edges.size() + volume.ext_vertices.size();
      for(const auto& face : volume.ext_faces)
        ix_total += (face.vertices.size() - 2)*3;
      for(const auto& face : volume.ext_edges)
        ix_total += (face.vertices.size() - 2)*3;
      for(const auto& face : volume.ext_vertices)
        ix_total += (face.vertices.size() - 2)*3;
    }
    coords.reserve(vx_total);
    normals.reserve(vx_total);
    indices.reserve(ix_total);
  }
  for(auto& volume : volumes) {
    using MoveIter = std::move_iterator<decltype(begin(volume.vertex_coords))>;
    std::copy(MoveIter(begin(volume.vertex_coords)), MoveIter(end(volume.vertex_coords)),
        std::back_inserter(coords));
    for(const auto& face : volume.ext_faces)
      std::fill_n(std::back_inserter(normals), face.vertices.size(), face.normal);
    append_face_list(indices, volume.ext_faces);
    append_face_list(indices, volume.ext_edges);
    append_face_list(indices, volume.ext_vertices);
  }
  std::cout << "[ ";
  for(auto ix : indices)
    std::cout << ix << ", ";
  std::cout << "]\n";
}
