// SPDX-License-Identifier: Apache-2.0

#include "neonstore/inventory.hpp"
#include <chrono>
#include <iostream>

int main() {
  using clock = std::chrono::high_resolution_clock;
  neonstore::Inventory inv;
  for (int i=0;i<10000;++i) {
    inv.add_product(neonstore::Product{.id="P"+std::to_string(i), .name="N", .price=1.0, .stock=100});
  }
  auto t0 = clock::now();
  for (int i=0;i<10000;++i) {
    inv.restock("P"+std::to_string(i), 1);
  }
  auto t1 = clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
  std::cout << "restock_total_ns=" << ns << "\n";
  return 0;
}
