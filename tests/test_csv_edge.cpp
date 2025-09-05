// SPDX-License-Identifier: Apache-2.0
#include "neonstore/csv_storage.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>

using namespace neonstore;
namespace fs = std::filesystem;

int main() {
  Inventory inv;
  inv.add_product(Product{.id="A,1", .name="A"B", .price=1.2, .stock=1});
  inv.add_product(Product{.id="B
2", .name="CD", .price=2.3, .stock=2});
  fs::path d = "data_edge";
  if (fs::exists(d)) fs::remove_all(d);
  CsvStorage::save_products(inv, d.string());
  Inventory inv2;
  CsvStorage::load_products(inv2, d.string());
  assert(inv2.find("A,1"));
  assert(inv2.find("B
2"));
  fs::remove_all(d);
  return 0;
}
