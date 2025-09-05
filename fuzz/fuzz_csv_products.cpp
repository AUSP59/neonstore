// SPDX-License-Identifier: Apache-2.0

#include "neonstore/csv_storage.hpp"
#include <fstream>
#include <vector>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  try {
    std::string s(reinterpret_cast<const char*>(data), size);
    std::ofstream("fuzz_tmp/products.csv") << "id,name,price,stock\n" << s;
    neonstore::Inventory inv;
    neonstore::CsvStorage::load_products(inv, "fuzz_tmp");
  } catch (...) {}
  return 0;
}
