// Smoke test: ensures the generated single-file `code.cpp` is a valid,
// self-contained translation unit that a judge main can build against.
#include "../code.cpp"

#include <cassert>
#include <sstream>

int main() {
  using sjtu::int2048;
  auto s = [](const int2048 &x) {
    std::ostringstream o;
    o << x;
    return o.str();
  };
  assert(s(int2048(10) / int2048(-3)) == "-4");
  assert(s(int2048(-10) % int2048(3)) == "2");
  assert(s(int2048(std::string("123456789123456789")) *
           int2048(std::string("1000000000"))) ==
         "123456789123456789000000000");
  int2048 a(7), b(3);
  a.add(b);
  assert(s(a) == "10");
  std::cout << "oj_smoke OK\n";
  return 0;
}
