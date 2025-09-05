// SPDX-License-Identifier: Apache-2.0
#include "neonstore/capi.h"
#include "neonstore/inventory.hpp"
#include <vector>
#include <string>

using namespace neonstore;
struct NSHandle { Inventory inv; std::vector<Product> cache; };

int ns_inventory_create(void** out_handle){
  if (!out_handle) return -1;
  try { *out_handle = new NSHandle(); return 0; } catch (...) { return -2; }
}
int ns_inventory_destroy(void* handle){
  if (!handle) return -1;
  delete static_cast<NSHandle*>(handle);
  return 0;
}
int ns_inventory_add_product(void* handle, ns_product_t p){
  if (!handle) return -1;
  try {
    Product pr;
    if (p.id) pr.id = p.id;
    if (p.name) pr.name = p.name;
    pr.price = p.price;
    pr.stock = p.stock;
    static_cast<NSHandle*>(handle)->inv.add_product(pr);
    return 0;
  } catch (...) { return -2; }
}
int ns_inventory_list_products(void* handle, ns_product_t* out, int cap){
  if (!handle) return -1;
  auto* h = static_cast<NSHandle*>(handle);
  h->cache = h->inv.list_products();
  if (!out || cap <= 0) return (int)h->cache.size();
  int n = std::min<int>(cap, (int)h->cache.size());
  for (int i=0;i<n;++i){
    out[i].id = h->cache[i].id.c_str();
    out[i].name = h->cache[i].name.c_str();
    out[i].price = h->cache[i].price;
    out[i].stock = h->cache[i].stock;
  }
  return (int)h->cache.size();
}
