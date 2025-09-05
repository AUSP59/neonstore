// SPDX-License-Identifier: Apache-2.0
#include "neonstore/csv_storage.hpp"
#include "neonstore/inventory.hpp"
#include <cassert>
#include <fstream>
#include <filesystem>
using namespace neonstore; namespace fs = std::filesystem;

int main(){
  Inventory inv; inv.add_product(Product{.id="P", .name="N", .price=1.0, .stock=1});
  fs::path d="data_int";
  if (fs::exists(d)) fs::remove_all(d);
  CsvIntegrity::set(Integrity::Lenient);
  CsvStorage::save_products(inv, d.string());
  // Tamper file
  std::ofstream(d.string()+"/products.csv", std::ios::app) << "\nP2,X,2.0,1\n";
  CsvIntegrity::set(Integrity::Strict);
  bool threw=false;
  try{ Inventory inv2; CsvStorage::load_products(inv2, d.string()); } catch(...){ threw=true; }
  assert(threw && "strict integrity should throw");
  fs::remove_all(d);
  return 0;
}
