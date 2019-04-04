#define DEBUG
#include "Mould.hpp"

int main() {
  Mould m{};
  m.cut({{1, 1, 1}, 0}, true);
  m.cut({{-1, 1, 1}, 0}, true);
  m.cut({{-1, 1, -1}, 0}, true);
  m.cut({{1, 1, -1}, 0}, true);
}
