// SPDX-License-Identifier: Apache-2.0
#include "neonstore/inventory.hpp"
#include <cassert>

using namespace neonstore;

int main(){
  Inventory inv;
  inv.add_product(Product{.id="P1", .name="N", .price=1.0, .stock=1});
  inv.set_price("P1", 2.5);
  const Product* p = inv.find("P1");
  assert(p && p->price == 2.5);
  inv.remove_product("P1");
  assert(inv.find("P1") == nullptr);
  return 0;
}
