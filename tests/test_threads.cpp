
// SPDX-License-Identifier: Apache-2.0
#include "neonstore/inventory.hpp"
#include <cassert>
#include <thread>
#include <vector>
using namespace neonstore;

int main() {
#ifdef NEONSTORE_THREADSAFE
  Inventory inv;
  inv.add_product(Product{.id="P", .name="N", .price=1.0, .stock=0});
  std::vector<std::thread> th;
  for (int i=0;i<8;++i) th.emplace_back([&]{ for (int j=0;j<1000;++j) inv.restock("P",1); });
  for (auto& t: th) t.join();
  const Product* p = inv.find("P");
  assert(p && p->stock==8000);
#endif
  return 0;
}
