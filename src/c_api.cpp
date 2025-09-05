
// SPDX-License-Identifier: Apache-2.0
#include "neonstore/inventory.hpp"
#include "neonstore/export.hpp"
#include "neonstore/c_api.h"
#include <new>
#include <string>

extern "C" {

neonstore_inventory_t neonstore_inventory_create(void){
  try { return reinterpret_cast<neonstore_inventory_t>(new neonstore::Inventory()); }
  catch (...) { return nullptr; }
}

void neonstore_inventory_destroy(neonstore_inventory_t inv){
  if (!inv) return;
  delete reinterpret_cast<neonstore::Inventory*>(inv);
}

int neonstore_inventory_add(neonstore_inventory_t inv, const char* id, const char* name, double price, int stock){
  if (!inv || !id || !name) return -1;
  try {
    neonstore::Product p;
    p.id = id; p.name = name; p.price = price; p.stock = stock;
    reinterpret_cast<neonstore::Inventory*>(inv)->add_product(p);
    return 0;
  } catch (...) { return -2; }
}

int neonstore_inventory_count(neonstore_inventory_t inv){
  if (!inv) return -1;
  auto* i = reinterpret_cast<neonstore::Inventory*>(inv);
  return static_cast<int>(i->list_products().size());
}

} // extern "C"
