// SPDX-License-Identifier: Apache-2.0
#include "neonstore/inventory.hpp"
#include <cassert>
#include <iostream>

using namespace neonstore;

int main() {
  Inventory inv;
  inv.add_product(Product{.id="P1", .name="Widget", .price=10.0, .stock=5});
  inv.restock("P1", 5);
  const Product* p = inv.find("P1");
  assert(p && p->stock == 10);
  auto order = inv.sell("P1", 3);
  assert(order.items.size()==1);
  assert(p->stock == 7);
  assert(order.total() == 30.0);
  std::cout << "OK\n";
  return 0;
}
