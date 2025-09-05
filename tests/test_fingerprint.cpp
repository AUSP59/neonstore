// SPDX-License-Identifier: Apache-2.0
#include "neonstore/sha256.hpp"
#include "neonstore/inventory.hpp"
#include "neonstore/csv_storage.hpp"
#include <cassert>
#include <filesystem>
using namespace neonstore;
int main(){
  Inventory inv; inv.add_product(Product{.id="A", .name="X", .price=1.0, .stock=1});
  std::filesystem::create_directories("dfp"); CsvStorage::save_products(inv, "dfp");
  Inventory inv2; CsvStorage::load_products(inv2, "dfp");
  auto v = inv2.list_products();
  std::string canon = std::string("P|"); for (auto& p:v){ canon += p.id + "|" + p.name + "|" + std::to_string(p.price) + "|" + std::to_string(p.stock) + "\n"; }
  auto h = Sha256::of_string(canon);
  assert(h.size()==64);
  return 0;
}
