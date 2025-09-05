// SPDX-License-Identifier: Apache-2.0
#include "neonstore/inventory.hpp"
#include <iostream>
int main(){ 
  neonstore::Inventory inv;
  inv.add_product(neonstore::Product{.id="P1", .name="Widget", .price=10.0, .stock=5});
  auto o = inv.sell("P1", 2);
  std::cout << o.total() << "\n";
  return 0;
}
