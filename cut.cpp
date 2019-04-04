#define DEBUG
#include "Mould.hpp"

int main() {
  Mould m{};
  m.cut({glm::normalize(glm::vec3{1, 1, 1}), 0}, true);
  m.cut({glm::normalize(glm::vec3{-1, 1, 1}), 0}, true);
  m.cut({glm::normalize(glm::vec3{-1, 1, -1}), 0}, true);
  m.cut({glm::normalize(glm::vec3{1, 1, -1}), 0}, true);
//m.clip_edges();
}
