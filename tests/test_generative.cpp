// SPDX-License-Identifier: Apache-2.0
#include "neonstore/inventory.hpp"
#include <cassert>
#include <random>
#include <string>
#include <unordered_set>

using namespace neonstore;

int main(){
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> stock(0, 1000);
  std::uniform_int_distribution<int> price_cents(1, 100000);
  std::unordered_set<std::string> ids;
  Inventory inv;

  for (int i=0;i<5000;++i){
    Product p;
    p.id = "P" + std::to_string(i);
    p.name = "Item" + std::to_string(i);
    p.price = price_cents(rng) / 100.0;
    p.stock = stock(rng);
    inv.add_product(p);
    ids.insert(p.id);
  }
  auto v = inv.list_products();
  assert(v.size() == ids.size()); // uniqueness preserved
  return 0;
}
