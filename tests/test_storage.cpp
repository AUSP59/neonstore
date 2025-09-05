// SPDX-License-Identifier: Apache-2.0

#include "neonstore/csv_storage.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace neonstore;
namespace fs = std::filesystem;

int main() {
  Inventory inv;
  inv.add_product(Product{.id="KBD", .name="Keyboard", .price=100.0, .stock=2});
  auto o = inv.sell("KBD", 1);
  std::vector<Order> orders{ o };

  fs::path dir = "data_test";
  if (fs::exists(dir)) fs::remove_all(dir);

  CsvStorage::save_products(inv, dir.string());
  CsvStorage::save_orders(orders, dir.string());

  Inventory inv2;
  std::vector<Order> orders2;
  CsvStorage::load_products(inv2, dir.string());
  CsvStorage::load_orders(orders2, dir.string());

  const Product* p2 = inv2.find("KBD");
  assert(p2 && p2->name == "Keyboard");
  assert(orders2.size()==1);
  assert(orders2[0].total()==100.0);

  fs::remove_all(dir);
  std::cout << "OK\n";
  return 0;
}
