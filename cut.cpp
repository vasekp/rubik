#define DEBUG
#include "Mould.hpp"

int main() {
  Mould m{};
  m.cut({{1, 0, 0}, 0}, true);
}
