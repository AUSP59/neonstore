// SPDX-License-Identifier: Apache-2.0

#include "neonstore/csv_storage.hpp"
#include <fstream>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  try {
    std::string s(reinterpret_cast<const char*>(data), size);
    std::ofstream("fuzz_tmp/orders.csv") << "id,product_id,quantity,price_each\n" << s;
    std::vector<neonstore::Order> orders;
    neonstore::CsvStorage::load_orders(orders, "fuzz_tmp");
  } catch (...) {}
  return 0;
}
